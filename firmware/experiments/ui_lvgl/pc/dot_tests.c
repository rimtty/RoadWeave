#include <stdio.h>
#include "scenarios.h"
#include "ui_dot.h"
#include "capture.h"

#define CHECK(x) do { if(!(x)) {fprintf(stderr,"Dot test failed at %d: %s\n",__LINE__,#x);return 1;} } while(0)
static unsigned actions;
static void action(const char *name,uint32_t arg) {(void)name;(void)arg;actions++;}
static void drag(int x,int y,int ex,int ey,int duration)
{
    lv_test_mouse_move_to(x,y);lv_test_mouse_press();lv_test_wait(40);
    for(int i=1;i<=6;i++) {lv_test_mouse_move_to(x+(ex-x)*i/6,y+(ey-y)*i/6);lv_test_wait((uint32_t)(duration/6));}
    lv_test_mouse_release();lv_test_wait(60);
}
static uint32_t frame(const ui_model_t *m)
{ui_update(m);lv_test_wait(50);return capture_frame_hash();}
int test_dot_interactions(void)
{
    ui_model_t m;scenario_fill("dot13",0,&m);ui_show(UI_SCREEN_DOT13);ui_update(&m);
    lv_test_indev_create_all();ui_set_action_cb(action);lv_test_wait(100);
    uint32_t first=capture_frame_hash();
    for(int i=1;i<=8;i++) {
        drag(200,150,35,150,180);CHECK(ui_current()==13+i%4);
    }
    CHECK(capture_frame_hash()==first);
    for(int i=1;i<=8;i++) {
        drag(35,150,200,150,180);CHECK(ui_current()==13+(4-i%4)%4);
    }
    CHECK(capture_frame_hash()==first);
    drag(120,150,140,150,180);CHECK(ui_current()==UI_SCREEN_DOT13); // too short
    drag(120,250,120,60,180);CHECK(ui_current()==UI_SCREEN_DOT13); // vertical
    drag(200,250,60,70,180);CHECK(ui_current()==UI_SCREEN_DOT13); // diagonal
    drag(200,150,35,150,1200);CHECK(ui_current()==UI_SCREEN_DOT13); // long hold
    lv_test_mouse_click_at(120,160);CHECK(ui_current()==UI_SCREEN_DOT13);
    lv_test_mouse_move_to(200,150);lv_test_mouse_press();lv_test_wait(50);
    lv_test_mouse_move_to(40,150);lv_test_wait(100);ui_dot_cancel_touch();
    lv_test_mouse_release();lv_test_wait(50);CHECK(ui_current()==UI_SCREEN_DOT13);
    // Switching away during a drag must not turn a later release into navigation.
    lv_test_mouse_move_to(200,150);lv_test_mouse_press();lv_test_wait(50);
    ui_show(UI_SCREEN_DOT15);lv_test_mouse_move_to(40,150);lv_test_wait(50);
    lv_test_mouse_release();lv_test_wait(50);CHECK(ui_current()==UI_SCREEN_DOT15);
    CHECK(actions==0); // touch gestures never start TX/mute
    for(int id=13;id<=16;id++) {
        ui_show((ui_screen_t)id);scenario_fill("dot13",0,&m);
        uint32_t normal=frame(&m);
        m.peers[0].dist_m=999;m.peers[1].dist_m=27;CHECK(frame(&m)!=normal);
        for(int i=0;i<3;i++)m.peers[i].age=POS_PEER_STALE;
        uint32_t stale=frame(&m);
        m.peers[0].dist_m=500;m.peers[1].dist_m=600;m.peers[2].dist_m=700;
        CHECK(frame(&m)==stale); // unknown positions must not leak old values
        m.link_ok=false;uint32_t offline=frame(&m);CHECK(offline!=stale);
        m.peers[0].dist_m=10000;CHECK(frame(&m)==offline);
        scenario_fill("dot13",0,&m);CHECK(frame(&m)==normal);
        m.voice=UI_VOICE_TX;CHECK(frame(&m)!=normal);
        m.voice=UI_VOICE_IDLE;CHECK(frame(&m)!=normal);
        // Public model boundary clamps out-of-range counts, including new pages.
        m.n_peers=99;frame(&m);m.n_peers=-1;frame(&m);
    }
    ui_show(UI_SCREEN_DOT14);scenario_fill("dot14",0,&m);uint32_t wave=frame(&m);
    uint32_t fixed=capture_outside_wave_hash();
    m.animation_ms=160;CHECK(frame(&m)!=wave);
    CHECK(capture_outside_wave_hash()==fixed);
    for(unsigned t=2;t<26;t++){m.animation_ms=t*160;frame(&m);CHECK(capture_outside_wave_hash()==fixed);}
    m.peers[0].muted=true;uint32_t quiet=frame(&m);
    m.animation_ms=640;CHECK(frame(&m)==quiet);
    m.voice=UI_VOICE_TX;uint32_t tx=frame(&m);m.animation_ms=800;CHECK(frame(&m)!=tx);
    m.voice=UI_VOICE_IDLE;quiet=frame(&m);m.animation_ms=960;CHECK(frame(&m)==quiet);
    puts("PASS: dot left/right wrap, rejected gestures, cancellation, state persistence, stale privacy, audio motion");
    return 0;
}
