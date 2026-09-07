#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <SDL.h>
#include "lvgl.h"
#include "scenarios.h"
#include "capture.h"
#include "ui_dot.h"

static bool quit_requested, pause_requested, reset_requested, save_requested, step_requested;
static int requested_screen = -1, requested_zoom;
static const char *requested_scenario;
static bool trace_input;
static bool printable_down[128];
static bool zoom_cycle_requested;

/* SDL event watches may run on another thread: this watch only handles events
 * originating in the main thread's SDL_PollEvent, and queues UI work for later. */
static SDL_threadID main_thread;
static int SDLCALL event_watch(void *unused, SDL_Event *event)
{
    (void)unused;
    if (trace_input && (event->type == SDL_KEYDOWN || event->type == SDL_KEYUP || event->type == SDL_TEXTINPUT)) {
        if (event->type == SDL_TEXTINPUT) printf("SDL text: %s\n", event->text.text);
        else printf("SDL key type=%u thread=%lu main=%lu key=%d repeat=%d\n", event->type,
                    SDL_ThreadID(), main_thread, event->key.keysym.sym, event->key.repeat);
        fflush(stdout);
    }
    if (SDL_ThreadID() != main_thread) return 1;
    if (event->type == SDL_QUIT ||
        (event->type == SDL_WINDOWEVENT && event->window.event == SDL_WINDOWEVENT_CLOSE))
        quit_requested = true;
    if (event->type == SDL_WINDOWEVENT && (event->window.event == SDL_WINDOWEVENT_FOCUS_LOST ||
        event->window.event == SDL_WINDOWEVENT_LEAVE))
        { memset(printable_down, 0, sizeof(printable_down)); ui_dot_cancel_touch(); }
    SDL_Keycode key;
    if (event->type == SDL_TEXTINPUT) {
        /* Accessibility / remote input can supply WM_CHAR without a physical
         * scan code. Accept single ASCII shortcuts, but suppress the text that
         * follows a regular keydown so Space does not toggle twice. */
        unsigned char c = (unsigned char)event->text.text[0];
        if (c >= 128 || c == 0 || event->text.text[1] != '\0') return 1;
        key = tolower(c);
        if (printable_down[key]) return 1;
    } else if (event->type == SDL_KEYDOWN || event->type == SDL_KEYUP) {
        key = event->key.keysym.sym;
        if (key >= 0 && key < 128) {
            key = tolower((unsigned char)key);
            printable_down[key] = event->type == SDL_KEYDOWN;
        }
        if (event->type == SDL_KEYUP || event->key.repeat) return 1;
    } else return 1;
    switch (key) {
        case SDLK_ESCAPE: quit_requested = true; break;
        case SDLK_SPACE: pause_requested = true; break;
        case SDLK_r: reset_requested = true; break;
        case SDLK_s: save_requested = true; break;
        case SDLK_PERIOD: step_requested = true; break;
        case SDLK_1: requested_screen = UI_SCREEN_GROUP; break;
        case SDLK_2: requested_screen = UI_SCREEN_RADAR; break;
        case SDLK_3: requested_screen = UI_SCREEN_CONVOY; break;
        case SDLK_4: requested_scenario = "dot13"; break;
        case SDLK_5: requested_scenario = "dot14"; break;
        case SDLK_6: requested_scenario = "dot15"; break;
        case SDLK_7: requested_scenario = "dot16"; break;
        case SDLK_F1: requested_zoom = 1; break;
        case SDLK_F2: requested_zoom = 2; break;
        case SDLK_F3: requested_zoom = 3; break;
        case SDLK_z: zoom_cycle_requested = true; break;
        case SDLK_0: requested_scenario = "empty"; break;
        case SDLK_8: requested_scenario = "max_group"; break;
        case SDLK_g: requested_scenario = ui_dot_is_screen(ui_current())?"dot_idle":"group"; break;
        case SDLK_d: requested_scenario = ui_dot_is_screen(ui_current())?"dot_stale":"stale"; break;
        case SDLK_l: requested_scenario = ui_dot_is_screen(ui_current())?"dot_offline":"link_down"; break;
        case SDLK_x:
            requested_scenario = ui_dot_is_screen(ui_current())?"dot13":"rx";
            if(ui_dot_is_screen(ui_current()))requested_screen=ui_current();
            break;
        case SDLK_t: requested_scenario = ui_dot_is_screen(ui_current())?"dot_tx":"tx"; break;
        case SDLK_b: requested_scenario = ui_dot_is_screen(ui_current())?"dot_busy":"busy"; break;
        case SDLK_m: sim_action("mute",0); break;
        default: break;
    }
    return 1;
}

static void pump_window(uint32_t ms)
{
    uint32_t start = SDL_GetTicks();
    while (SDL_GetTicks() - start < ms) { lv_timer_handler(); SDL_Delay(5); }
}

static void on_display_resize(lv_event_t *event)
{
    lv_display_t *display = lv_event_get_target(event);
    /* SDL reallocates the buffer even when only zoom changes. Invalidate the
     * whole screen so unchanged controls are restored along with live labels. */
    lv_obj_invalidate(lv_display_get_screen_active(display));
}

static bool sdl_drag(uint32_t id,int zoom,int x,int y,int ex,int ey)
{
    SDL_Event e={0};e.type=SDL_MOUSEBUTTONDOWN;e.button.windowID=id;
    e.button.button=SDL_BUTTON_LEFT;e.button.state=SDL_PRESSED;e.button.x=x*zoom;e.button.y=y*zoom;
    if(SDL_PushEvent(&e)!=1)return false;pump_window(60);
    for(int i=1;i<=6;i++) {
        e=(SDL_Event){0};e.type=SDL_MOUSEMOTION;e.motion.windowID=id;e.motion.state=SDL_BUTTON_LMASK;
        e.motion.x=(x+(ex-x)*i/6)*zoom;e.motion.y=(y+(ey-y)*i/6)*zoom;
        if(SDL_PushEvent(&e)!=1)return false;pump_window(30);
    }
    e=(SDL_Event){0};e.type=SDL_MOUSEBUTTONUP;e.button.windowID=id;e.button.button=SDL_BUTTON_LEFT;
    e.button.state=SDL_RELEASED;e.button.x=ex*zoom;e.button.y=ey*zoom;
    if(SDL_PushEvent(&e)!=1)return false;pump_window(60);return true;
}

/* Check the SDL renderer as well as LVGL's memory: a correct logical PNG alone
 * cannot detect presentation problems in the desktop backend. */
static bool renderer_matches_frame(lv_display_t *display)
{
    SDL_Renderer *renderer=lv_sdl_window_get_renderer(display);
    int w,h;if(SDL_GetRendererOutputSize(renderer,&w,&h)!=0)return false;
    uint16_t *pixels=malloc((size_t)w*(size_t)h*2);if(!pixels)return false;
    bool ok=SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_RGB565,pixels,w*2)==0;
    const lv_draw_buf_t *b=lv_display_get_buf_active(display);
    if(ok)for(int y=0;y<320&&ok;y++)for(int x=0;x<240;x++) {
        uint16_t expected=((const uint16_t *)(b->data+y*b->header.stride))[x];
        int sx=(2*x+1)*w/(2*240),sy=(2*y+1)*h/(2*320);
        if(pixels[sy*w+sx]!=expected){ok=false;break;}
    }
    free(pixels);if(!ok)fputs("SDL renderer differs from LVGL logical frame\n",stderr);return ok;
}

static int test_window(lv_display_t *display)
{
    /* Exercise the real SDL driver and pixel-to-touch coordinate conversion.
     * CTest uses SDL's dummy video driver; the same test can open a real window. */
    uint32_t id = SDL_GetWindowID(lv_sdl_window_get_window(display));
    pump_window(150);
    for (int zoom = 1; zoom <= 3; zoom++) {
        uint32_t before_zoom = capture_frame_hash();
        lv_sdl_window_set_zoom(display, (float)zoom);
        pump_window(100);
        if (before_zoom != capture_frame_hash()) {
            fprintf(stderr, "Zoom %d changed the logical framebuffer\n", zoom); return 1;
        }
        if (lv_display_get_horizontal_resolution(display) != 240 ||
            lv_display_get_vertical_resolution(display) != 320) return 1;
        SDL_Event event = {0};
        event.type = SDL_MOUSEBUTTONDOWN;
        event.button.windowID = id; event.button.button = SDL_BUTTON_LEFT;
        event.button.state = SDL_PRESSED;
        event.button.x = 208 * zoom; event.button.y = 285 * zoom;
        if (SDL_PushEvent(&event) != 1) return 1;
        pump_window(100);
        event.type = SDL_MOUSEBUTTONUP; event.button.state = SDL_RELEASED;
        if (SDL_PushEvent(&event) != 1) return 1;
        pump_window(100);
        if (ui_current() != zoom % UI_SCREEN_COUNT) {
            fprintf(stderr, "SDL mouse mapping failed at zoom %d\n", zoom); return 1;
        }
    }
    SDL_Event key = {0};
    key.type = SDL_KEYDOWN; key.key.windowID = id; key.key.keysym.sym = SDLK_SPACE;
    if (SDL_PushEvent(&key) != 1) return 1;
    pump_window(80);
    if (!pause_requested || !capture_png("window.png")) return 1;
    pause_requested = false;
    SDL_Event text = {0}; text.type = SDL_TEXTINPUT; text.text.windowID = id;
    strcpy(text.text.text, " ");
    SDL_PushEvent(&text); pump_window(50);
    if (pause_requested) { fputs("Duplicate keydown/text handling\n", stderr); return 1; }
    key.type = SDL_KEYUP; SDL_PushEvent(&key); pump_window(50);
    SDL_PushEvent(&text); pump_window(50);
    if (!pause_requested) { fputs("Text-only Space did not arrive\n", stderr); return 1; }
    strcpy(text.text.text, "Z"); SDL_PushEvent(&text); pump_window(50);
    if (!zoom_cycle_requested) return 1;
    strcpy(text.text.text, "2"); SDL_PushEvent(&text); pump_window(50);
    if (requested_screen != UI_SCREEN_RADAR) return 1;
    ui_model_t dot_model;scenario_fill("dot13",0,&dot_model);
    ui_show(UI_SCREEN_DOT13);ui_update(&dot_model);pump_window(100);
    for(int z=1;z<=3;z++) {
        uint32_t before=capture_frame_hash();
        lv_sdl_window_set_zoom(display,(float)z);pump_window(100);
        if(capture_frame_hash()!=before)return 1;
        for(int i=1;i<=4;i++) {
            if(!sdl_drag(id,z,200,150,35,150)||ui_current()!=13+i%4)return 1;
        }
        for(int i=1;i<=4;i++) {
            if(!sdl_drag(id,z,35,150,200,150)||ui_current()!=13+(4-i%4)%4)return 1;
        }
        if(capture_frame_hash()!=before)return 1;
        if(!sdl_drag(id,z,120,230,120,60)||ui_current()!=UI_SCREEN_DOT13)return 1;
        char file[40];snprintf(file,sizeof(file),"dot-window-%dx.png",z);
        if(!capture_png(file))return 1;
    }
    ui_show(UI_SCREEN_DOT14);ui_update(&dot_model);pump_window(100);
    uint32_t fixed=capture_outside_wave_hash();
    for(unsigned phase=1;phase<=26;phase++) {
        dot_model.animation_ms=phase*160;ui_update(&dot_model);pump_window(35);
        if(capture_outside_wave_hash()!=fixed) {
            fputs("SDL partial refresh changed content outside waveform\n",stderr);return 1;
        }
        if(!renderer_matches_frame(display))return 1;
    }
    dot_model.peers[0].muted=true;ui_update(&dot_model);pump_window(80);
    if(!renderer_matches_frame(display))return 1;
    dot_model.voice=UI_VOICE_TX;ui_update(&dot_model);pump_window(80);
    if(!renderer_matches_frame(display))return 1;
    dot_model.link_ok=false;ui_update(&dot_model);pump_window(80);
    if(!renderer_matches_frame(display))return 1;
    puts("PASS: SDL zoom/mouse/keyboard/text input, duplicate suppression, PNG export");
    puts("PASS: Dot forward/backward swipes and vertical rejection at 1x/2x/3x");
    puts("PASS: SDL renderer readback matches RGB565 frame through waveform/mute/TX/offline updates");
    return 0;
}

static void help(void)
{
    puts("RoadWeave LVGL simulator (240x320 RGB565)\n"
         "  --scenario NAME   group radar convoy empty max_group max_convoy stale\n"
         "                    radar_east radar_far rx tx busy link_down\n"
         "  --capture FILE    Render a deterministic headless PNG, then exit\n"
         "  --reference FILE  Compare with an existing baseline (requires --capture)\n"
        "  --test-input      Run deterministic touch tests, then exit\n"
         "  --test-dot        Run dot swipe/state tests, then exit\n"
         "  --test-window     Test SDL input at all zoom levels, then exit\n"
         "  --zoom 1|2|3      Interactive zoom (default 2)\n"
         "  --paused          Start interactive mode paused\n"
         "  --seconds N       Auto-close interactive mode after N real seconds\n"
         "  --trace-input     Print SDL keyboard events for input diagnostics\n"
         "Keys: 1/2/3 legacy; 4/5/6/7 Dot 13-16; F1/F2/F3 or Z zoom; Space pause; . step; S PNG\n"
         "      Dot: swipe left = next, right = previous (wrap); M mute; R reset\n"
         "      G normal; 0 empty; 8 max peers; D stale; L link down; X RX; T TX; B busy\n"
         "Mouse: touch buttons; drag lists to scroll; Esc closes.\n"
         "PTT/mute affect simulated data only. No radio, USB or credentials are used.");
}

int main(int argc, char **argv)
{
    const char *scenario = "group", *output = NULL, *reference = NULL;
    bool inputs = false, paused = false, window_test = false, dot_inputs = false;
    int zoom = 2;
    double duration = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--help")) { help(); return 0; }
        else if (!strcmp(argv[i], "--test-input")) inputs = true;
        else if (!strcmp(argv[i], "--test-dot")) { inputs = true; dot_inputs = true; }
        else if (!strcmp(argv[i], "--test-window")) window_test = true;
        else if (!strcmp(argv[i], "--paused")) paused = true;
        else if (!strcmp(argv[i], "--trace-input")) trace_input = true;
        else if (!strcmp(argv[i], "--scenario") && i + 1 < argc) scenario = argv[++i];
        else if (!strcmp(argv[i], "--capture") && i + 1 < argc) output = argv[++i];
        else if (!strcmp(argv[i], "--reference") && i + 1 < argc) reference = argv[++i];
        else if (!strcmp(argv[i], "--zoom") && i + 1 < argc) zoom = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seconds") && i + 1 < argc) duration = atof(argv[++i]);
        else { help(); return 2; }
    }
    if (!scenario_valid(scenario) || zoom < 1 || zoom > 3 || duration < 0 ||
        (reference && !output) || (inputs && output) || (window_test && (inputs || output))) { help(); return 2; }
    bool headless = output || inputs;
    lv_init();
    lv_display_t *display;
    if (headless) {
        display = lv_test_display_create(240, 320);
        lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    } else {
        SDL_SetMainReady();
        display = lv_sdl_window_create(240, 320);
        if (!display || !lv_sdl_window_get_window(display)) {
            fprintf(stderr, "SDL window failed: %s\n", SDL_GetError());
            return 1;
        }
        lv_sdl_window_set_zoom(display, (float)zoom);
        lv_sdl_window_set_resizeable(display, false);
        lv_sdl_mouse_create();
        SDL_StartTextInput();
        main_thread = SDL_ThreadID();
        SDL_AddEventWatch(event_watch, NULL);
        help();
    }
    ui_set_action_cb(sim_action);
    ui_create(display);
    if (!headless) lv_display_add_event_cb(display, on_display_resize, LV_EVENT_RESOLUTION_CHANGED, NULL);
    ui_show(scenario_screen(scenario));
    ui_model_t model;
    scenario_fill(scenario, 0, &model);
    ui_update(&model);

    if (window_test) {
        int result = test_window(display);
        SDL_DelEventWatch(event_watch, NULL);
        lv_deinit();
        return result;
    }

    if (headless) {
        int result = 0;
        lv_test_wait(300);
        if (inputs) result = dot_inputs ? test_dot_interactions() : test_interactions();
        else {
            result = test_scenario(scenario, &model);
            if (!capture_png(output)) result = 1;
            if (reference && lv_test_screenshot_compare(reference) != LV_TEST_SCREENSHOT_RESULT_PASSED) {
                fprintf(stderr, "FAIL: %s differs from baseline or baseline is missing\n", scenario);
                capture_diff(reference, "diff.png");
                result = 1;
            }
            if (!result) printf("PASS: %s -> %s\n", scenario, output);
        }
        lv_deinit();
        return result;
    }

    uint32_t start = SDL_GetTicks(), last = start, screenshot_number = 0;
    float sim_time = 0;
    while (!quit_requested) {
        lv_timer_handler(); /* Includes SDL event polling. */
        if (quit_requested) break;
        uint32_t now = SDL_GetTicks();
        if (duration > 0 && (now - start) / 1000.0 >= duration) break;
        if (pause_requested) { paused = !paused; pause_requested = false; }
        if (!paused) sim_time += (now - last) / 1000.0f;
        last = now;
        if (step_requested) { paused = true; sim_time += 0.1f; step_requested = false; }
        if (reset_requested) { sim_time = 0; sim_reset_controls(); reset_requested = false; }
        if (requested_scenario) {
            ui_screen_t previous = ui_current();
            scenario = requested_scenario; requested_scenario = NULL;
            sim_time = 0; sim_reset_controls(); ui_show(scenario_screen(scenario));
            if (ui_dot_is_screen(previous) && strncmp(scenario,"dot_",4)==0) ui_show(previous);
        }
        if (requested_screen >= 0) { ui_show((ui_screen_t)requested_screen); requested_screen = -1; }
        if (requested_zoom) { zoom = requested_zoom; lv_sdl_window_set_zoom(display, (float)zoom); requested_zoom = 0; }
        if (zoom_cycle_requested) {
            zoom = zoom % 3 + 1; lv_sdl_window_set_zoom(display, (float)zoom); zoom_cycle_requested = false;
        }
        scenario_fill(scenario, sim_time, &model);
        sim_apply_controls(&model);
        ui_update(&model);
        char screen_name[24];
        if(ui_dot_is_screen(ui_current()))snprintf(screen_name,sizeof(screen_name),"dot%d",ui_current());
        else snprintf(screen_name,sizeof(screen_name),"%s",scenario);
        if (save_requested) {
            char filename[128];
            snprintf(filename, sizeof(filename), "capture-%s-%u-%u.png", screen_name, start, ++screenshot_number);
            if (capture_png(filename)) printf("Saved %s\n", filename);
            save_requested = false;
        }
        char title[180];
        snprintf(title, sizeof(title), "RoadWeave | %s | %.1fs %s | %dx | Space:pause S:PNG 1/2/3:screen",
                 screen_name, (double)sim_time, paused ? "PAUSED" : "RUNNING", zoom);
        lv_sdl_window_set_title(display, title);
        SDL_Delay(10);
    }
    SDL_DelEventWatch(event_watch, NULL);
    lv_deinit();
    return 0;
}
