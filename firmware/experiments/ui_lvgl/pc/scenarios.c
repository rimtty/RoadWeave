#include <math.h>
#include <stdio.h>
#include <string.h>
#include "scenarios.h"
#include "ui_simulation.h"

static bool ptt, muted;
static int downs, ups, mutes;
static const char *const names[] = {
    "group", "radar", "convoy", "empty", "max_group", "max_convoy", "stale",
    "radar_east", "radar_far", "rx", "tx", "busy", "link_down"
};

bool scenario_valid(const char *name)
{
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
        if (strcmp(name, names[i]) == 0) return true;
    return false;
}

ui_screen_t scenario_screen(const char *name)
{
    if (strncmp(name, "radar", 5) == 0) return UI_SCREEN_RADAR;
    if (strcmp(name, "convoy") == 0 || strcmp(name, "max_convoy") == 0 ||
        strcmp(name, "stale") == 0) return UI_SCREEN_CONVOY;
    return UI_SCREEN_GROUP;
}

void scenario_fill(const char *name, float seconds, ui_model_t *m)
{
    ui_simulate(m, seconds);
    m->voice = UI_VOICE_IDLE;
    for (int i = 0; i < m->n_peers; i++) m->peers[i].talking = false;
    if (strcmp(name, "empty") == 0) m->n_peers = 0;
    if (strncmp(name, "max_", 4) == 0) {
        m->n_peers = UI_MAX_PEERS;
        for (int i = 0; i < m->n_peers; i++) {
            ui_peer_t *p = &m->peers[i];
            memset(p, 0, sizeof(*p));
            snprintf(p->callsign, sizeof(p->callsign), "CAR%04d", i + 1);
            p->id = (uint32_t)(0x101 + i);
            p->along_m = 350.0f - i * 100.0f;
            p->dist_m = fabsf(p->along_m);
            p->bearing_deg = p->along_m >= 0 ? m->heading_deg : m->heading_deg + 180;
            p->rssi_dbm = (int8_t)(-60 - 4 * i);
            p->age = POS_PEER_FRESH;
        }
    }
    if (strcmp(name, "stale") == 0) {
        m->peers[1].age = POS_PEER_DIM;
        m->peers[2].age = POS_PEER_STALE;
    }
    if (strcmp(name, "radar_east") == 0) {
        m->heading_deg = 90;
        m->peers[0].bearing_deg = 90;
        m->peers[0].dist_m = 200;
        m->peers[1].bearing_deg = 0;
        m->peers[1].dist_m = 350;
        m->peers[2].bearing_deg = 180;
        m->peers[2].dist_m = 400;
    }
    if (strcmp(name, "radar_far") == 0) m->peers[2].dist_m = 3000;
    if (strcmp(name, "rx") == 0) {
        m->voice = UI_VOICE_RX;
        m->peers[0].talking = true;
        strcpy(m->talker, "LOTUS");
    }
    if (strcmp(name, "tx") == 0) m->voice = UI_VOICE_TX;
    if (strcmp(name, "busy") == 0) m->voice = UI_VOICE_BUSY;
    if (strcmp(name, "link_down") == 0) {
        m->link_ok = false;
        m->link_rssi_dbm = -99;
        m->battery_pct = 5;
    }
}

void sim_reset_controls(void) { ptt = muted = false; downs = ups = mutes = 0; }

void sim_action(const char *action, uint32_t arg)
{
    (void)arg;
    if (strcmp(action, "ptt_down") == 0) { ptt = true; downs++; }
    if (strcmp(action, "ptt_up") == 0) { ptt = false; ups++; }
    if (strcmp(action, "mute") == 0) { muted = !muted; mutes++; }
    printf("action: %s\n", action);
}

void sim_apply_controls(ui_model_t *m)
{
    if (ptt) m->voice = UI_VOICE_TX;
    for (int i = 0; i < m->n_peers; i++) m->peers[i].muted = muted;
}

static lv_obj_t *find_text(lv_obj_t *obj, const char *text)
{
    if (lv_obj_check_type(obj, &lv_label_class) &&
        strstr(lv_label_get_text(obj), text)) return obj;
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) {
        lv_obj_t *found = find_text(lv_obj_get_child(obj, (int32_t)i), text);
        if (found) return found;
    }
    return NULL;
}
#define contains_text(obj, text) (find_text(obj, text) != NULL)

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; } } while (0)

int test_scenario(const char *name, const ui_model_t *m)
{
    CHECK(ui_current() == scenario_screen(name));
    CHECK(contains_text(lv_screen_active(), "GROUP A"));
    lv_obj_t *battery = find_text(lv_screen_active(), "%");
    lv_obj_t *link = find_text(lv_screen_active(), LV_SYMBOL_WIFI);
    CHECK(battery && link);
    lv_area_t battery_area, link_area;
    lv_obj_get_coords(battery, &battery_area); lv_obj_get_coords(link, &link_area);
    CHECK(battery_area.x2 < 240 && battery_area.y2 < 28);
    CHECK(link_area.x1 >= 0 && link_area.y1 >= 0 && link_area.x2 < battery_area.x1);
    if (strcmp(name, "empty") == 0) CHECK(contains_text(lv_screen_active(), "1 NODES"));
    if (strcmp(name, "max_group") == 0) {
        CHECK(contains_text(lv_screen_active(), "9 NODES"));
        CHECK(contains_text(lv_screen_active(), "CAR0008"));
    }
    if (strcmp(name, "rx") == 0) CHECK(contains_text(lv_screen_active(), "LOTUS"));
    if (strcmp(name, "tx") == 0) CHECK(contains_text(lv_screen_active(), "YOU (TX)"));
    if (strcmp(name, "busy") == 0) CHECK(contains_text(lv_screen_active(), "BUSY"));
    if (strcmp(name, "stale") == 0) CHECK(contains_text(lv_screen_active(), "--- x"));
    if (strcmp(name, "radar_far") == 0) CHECK(contains_text(lv_screen_active(), "2000 m"));
    if (strcmp(name, "link_down") == 0) CHECK(!m->link_ok);
    return 0;
}

int test_interactions(void)
{
    ui_model_t m;
    scenario_fill("group", 0, &m);
    ui_update(&m);
    lv_test_indev_create_all();
    lv_test_wait(300);
    for (int i = 1; i <= UI_SCREEN_COUNT; i++) {
        lv_test_mouse_click_at(208, 285);
        CHECK(ui_current() == i % UI_SCREEN_COUNT);
    }
    lv_test_mouse_move_to(120, 285);
    lv_test_mouse_press();
    lv_test_wait(80);
    CHECK(ptt && downs == 1 && ups == 0);
    sim_apply_controls(&m);
    ui_update(&m);
    CHECK(contains_text(lv_screen_active(), "YOU (TX)"));
    lv_test_wait(1000);
    CHECK(downs == 1); /* Holding must not start transmission repeatedly. */
    lv_test_mouse_release();
    lv_test_wait(80);
    CHECK(!ptt && ups == 1);
    lv_test_mouse_press();
    lv_test_wait(80);
    CHECK(ptt && downs == 2);
    lv_test_mouse_move_to(120, 180);
    lv_test_wait(80);
    CHECK(!ptt && ups == 2); /* Dragging off must release the transmit floor. */
    lv_test_mouse_release();
    lv_test_wait(80);
    CHECK(ups == 2);
    lv_test_mouse_click_at(30, 285);
    CHECK(muted && mutes == 1);
    lv_test_mouse_click_at(30, 285);
    CHECK(!muted && mutes == 2);
    lv_test_mouse_move_to(120, 285);
    lv_test_mouse_press(); lv_test_wait(80);
    CHECK(ptt);
    ui_show(UI_SCREEN_CONVOY);
    CHECK(!ptt && downs == 3 && ups == 3);
    lv_test_mouse_release(); lv_test_wait(80);
    scenario_fill("max_convoy", 0, &m); ui_update(&m); lv_test_wait(100);
    lv_obj_t *last_car = find_text(lv_screen_active(), "CAR0008");
    CHECK(last_car);
    lv_obj_t *list = lv_obj_get_parent(last_car);
    lv_area_t list_area;
    lv_obj_get_coords(list, &list_area);
    CHECK(list_area.y2 < 222); /* List content is clipped above the footer/buttons. */
    lv_test_mouse_move_to(120, 195); lv_test_mouse_press(); lv_test_wait(80);
    for (int i = 0; i < 8; i++) { lv_test_mouse_move_by(0, -12); lv_test_wait(30); }
    lv_test_mouse_release(); lv_test_wait(100);
    CHECK(lv_obj_get_scroll_y(list) > 0);
    lv_area_t last_area;
    lv_obj_get_coords(last_car, &last_area);
    CHECK(last_area.y1 >= list_area.y1 && last_area.y2 <= list_area.y2);
    lv_test_mouse_click_at(208, 285);
    CHECK(ui_current() == UI_SCREEN_GROUP); /* Scrolling must not move the controls. */
    m.n_peers = 99; ui_update(&m); lv_test_wait(100);
    CHECK(contains_text(lv_screen_active(), "9 NODES"));
    m.n_peers = -1; ui_update(&m); lv_test_wait(100);
    CHECK(contains_text(lv_screen_active(), "1 NODES"));
    puts("PASS: screen cycle, PTT hold/release/drag-off/screen-change, mute, list scroll, peer bounds");
    return 0;
}
