// RoadWeave UI screens (LVGL 9): GROUP (who is talking), RADAR (heading-up), CONVOY (ahead/behind list).
// Driving-first rules: big text, one glance, no deep menus. Touch: PTT (hold), MUTE, NEXT screen.
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "ui_screens.h"

static lv_obj_t *s_scr[UI_SCREEN_COUNT];
static ui_screen_t s_cur = UI_SCREEN_GROUP;
static ui_action_cb_t s_cb;
static bool s_ptt_pressed;

// shared top bar per screen
typedef struct { lv_obj_t *group, *link, *batt, *voice; } topbar_t;
static topbar_t s_top[UI_SCREEN_COUNT];

// GROUP screen
static lv_obj_t *s_group_rows[UI_MAX_PEERS];
static lv_obj_t *s_group_big;
// RADAR
static lv_obj_t *s_radar_dots[UI_MAX_PEERS]; static lv_obj_t *s_radar_lbl[UI_MAX_PEERS]; static lv_obj_t *s_radar_scale;
// CONVOY
static lv_obj_t *s_convoy_rows[UI_MAX_PEERS + 1]; static lv_obj_t *s_convoy_spread;

static const lv_color_t C_BG = LV_COLOR_MAKE(0x10, 0x12, 0x16);
static const lv_color_t C_TX = LV_COLOR_MAKE(0xE0, 0x30, 0x30);
static const lv_color_t C_RX = LV_COLOR_MAKE(0x30, 0x90, 0xE0);
static const lv_color_t C_OK = LV_COLOR_MAKE(0x30, 0xC0, 0x60);
static const lv_color_t C_DIM = LV_COLOR_MAKE(0x70, 0x74, 0x7A);

void ui_set_action_cb(ui_action_cb_t cb) { s_cb = cb; }
static void act(const char *a, uint32_t arg) { if (s_cb) s_cb(a, arg); }

static void btn_event(lv_event_t *e)
{
    const char *name = lv_event_get_user_data(e);
    lv_event_code_t c = lv_event_get_code(e);
    if (strcmp(name, "ptt") == 0) {
        if (c == LV_EVENT_PRESSED && !s_ptt_pressed) { s_ptt_pressed = true; act("ptt_down", 0); }
        else if ((c == LV_EVENT_RELEASED || c == LV_EVENT_PRESS_LOST) && s_ptt_pressed) {
            s_ptt_pressed = false; act("ptt_up", 0);
        }
        return;
    }
    if (c == LV_EVENT_CLICKED) { if (strcmp(name, "next") == 0) ui_show((s_cur + 1) % UI_SCREEN_COUNT); else act(name, 0); }
}

static lv_obj_t *make_btn(lv_obj_t *parent, const char *label, const char *action, lv_color_t color, int w)
{
    lv_obj_t *b = lv_button_create(parent);
    /* LVGL locks presses to their original object by default. PTT must release
     * immediately when the pointer leaves its hit area. */
    if (strcmp(action, "ptt") == 0) lv_obj_remove_flag(b, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_set_size(b, w, 56);
    lv_obj_set_style_bg_color(b, color, 0);
    lv_obj_set_style_radius(b, 10, 0);
    lv_obj_t *l = lv_label_create(b); lv_label_set_text(l, label); lv_obj_set_style_text_font(l, &lv_font_montserrat_20, 0); lv_obj_center(l);
    lv_obj_add_event_cb(b, btn_event, LV_EVENT_ALL, (void *)action);
    return b;
}

static void make_topbar(lv_obj_t *scr, topbar_t *t)
{
    lv_obj_t *bar = lv_obj_create(scr); lv_obj_set_size(bar, 240, 28); lv_obj_set_pos(bar, 0, 0);
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(bar, lv_color_black(), 0); lv_obj_set_style_border_width(bar, 0, 0); lv_obj_set_style_pad_all(bar, 2, 0); lv_obj_set_style_radius(bar, 0, 0);
    t->group = lv_label_create(bar); lv_label_set_text(t->group, "GROUP");
    t->voice = lv_label_create(bar); lv_label_set_text(t->voice, "");
    t->link = lv_label_create(bar); lv_label_set_text(t->link, LV_SYMBOL_WIFI);
    t->batt = lv_label_create(bar); lv_label_set_text(t->batt, "100%");
    for (int i = 0; i < 4; i++) lv_obj_set_style_text_color(((lv_obj_t *[]){t->group, t->voice, t->link, t->batt})[i], lv_color_white(), 0);
    /* Explicit columns prevent a wider BUSY label from overlapping the group. */
    lv_obj_set_pos(t->group, 4, 4); lv_obj_set_width(t->group, 76);
    lv_label_set_long_mode(t->group, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(t->voice, 80, 4); lv_obj_set_width(t->voice, 52);
    lv_obj_set_style_text_align(t->voice, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(t->link, 137, 4); lv_obj_set_width(t->link, 56);
    lv_obj_set_pos(t->batt, 196, 4); lv_obj_set_width(t->batt, 34);
    lv_obj_set_style_text_align(t->batt, LV_TEXT_ALIGN_RIGHT, 0);
}

static lv_obj_t *make_screen(ui_screen_t which)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(scr, C_BG, 0);
    make_topbar(scr, &s_top[which]);
    // bottom button row: MUTE | PTT (wide) | NEXT
    lv_obj_t *row = lv_obj_create(scr); lv_obj_set_size(row, 240, 64); lv_obj_set_pos(row, 0, 320 - 64);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(row, 0, 0); lv_obj_set_style_pad_all(row, 4, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW); lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    make_btn(row, LV_SYMBOL_MUTE, "mute", C_DIM, 56);
    make_btn(row, "PTT", "ptt", C_TX, 104);
    make_btn(row, LV_SYMBOL_RIGHT, "next", C_DIM, 56);
    return scr;
}

static lv_obj_t *make_list(lv_obj_t *scr, int y, int height)
{
    lv_obj_t *list = lv_obj_create(scr);
    lv_obj_set_pos(list, 8, y); lv_obj_set_size(list, 224, height);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 0, 0);
    lv_obj_set_style_radius(list, 0, 0);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_remove_flag(list, LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM);
    return list;
}

static void build_group(void)
{
    lv_obj_t *scr = s_scr[UI_SCREEN_GROUP];
    s_group_big = lv_label_create(scr); lv_obj_set_style_text_font(s_group_big, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(s_group_big, lv_color_white(), 0); lv_obj_set_pos(s_group_big, 8, 34); lv_label_set_text(s_group_big, "3 NODES ONLINE");
    lv_obj_t *list = make_list(scr, 72, 180);
    for (int i = 0; i < UI_MAX_PEERS; i++) {
        lv_obj_t *l = lv_label_create(list); lv_obj_set_style_text_font(l, &lv_font_montserrat_20, 0);
        lv_obj_set_width(l, 216); lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_color(l, lv_color_white(), 0); lv_obj_set_pos(l, 4, i * 26); lv_label_set_text(l, ""); s_group_rows[i] = l;
    }
}

static void build_radar(void)
{
    lv_obj_t *scr = s_scr[UI_SCREEN_RADAR];
    // rings
    for (int r = 1; r <= 3; r++) {
        lv_obj_t *c = lv_obj_create(scr); int d = r * 56; lv_obj_set_size(c, d, d); lv_obj_set_pos(c, 120 - d / 2, 145 - d / 2);
        lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, 0); lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_color(c, C_DIM, 0); lv_obj_set_style_border_width(c, 1, 0); lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    }
    lv_obj_t *me = lv_obj_create(scr); lv_obj_set_size(me, 12, 12); lv_obj_set_pos(me, 114, 139);
    lv_obj_set_style_radius(me, LV_RADIUS_CIRCLE, 0); lv_obj_set_style_bg_color(me, C_OK, 0); lv_obj_set_style_border_width(me, 0, 0);
    lv_obj_t *n = lv_label_create(scr); lv_label_set_text(n, "UP = HEADING"); lv_obj_set_style_text_color(n, lv_color_white(), 0); lv_obj_align(n, LV_ALIGN_TOP_MID, 0, 36);
    s_radar_scale = lv_label_create(scr); lv_label_set_text(s_radar_scale, "outer ring 250 m"); lv_obj_set_style_text_color(s_radar_scale, C_DIM, 0); lv_obj_set_pos(s_radar_scale, 8, 238);
    for (int i = 0; i < UI_MAX_PEERS; i++) {
        lv_obj_t *d = lv_obj_create(scr); lv_obj_set_size(d, 14, 14); lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(d, 0, 0); lv_obj_add_flag(d, LV_OBJ_FLAG_HIDDEN); s_radar_dots[i] = d;
        lv_obj_t *l = lv_label_create(scr); lv_obj_set_width(l, 100); lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_color(l, lv_color_white(), 0); lv_obj_add_flag(l, LV_OBJ_FLAG_HIDDEN); s_radar_lbl[i] = l;
    }
}

static void build_convoy(void)
{
    lv_obj_t *scr = s_scr[UI_SCREEN_CONVOY];
    lv_obj_t *list = make_list(scr, 36, 184);
    for (int i = 0; i <= UI_MAX_PEERS; i++) {
        lv_obj_t *l = lv_label_create(list); lv_obj_set_style_text_font(l, &lv_font_montserrat_20, 0);
        lv_obj_set_width(l, 216); lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_color(l, lv_color_white(), 0); lv_obj_set_pos(l, 4, i * 26); lv_label_set_text(l, ""); s_convoy_rows[i] = l;
    }
    s_convoy_spread = lv_label_create(scr); lv_obj_set_style_text_color(s_convoy_spread, C_DIM, 0); lv_obj_set_pos(s_convoy_spread, 12, 222); lv_label_set_text(s_convoy_spread, "");
}

void ui_create(lv_display_t *disp)
{
    if (disp) lv_display_set_default(disp);
    s_ptt_pressed = false;
    for (int i = 0; i < UI_SCREEN_COUNT; i++) s_scr[i] = make_screen((ui_screen_t)i);
    build_group(); build_radar(); build_convoy();
    ui_show(UI_SCREEN_GROUP);
}

void ui_show(ui_screen_t s) {
    if (s < 0 || s >= UI_SCREEN_COUNT) return;
    if (s_ptt_pressed) { s_ptt_pressed = false; act("ptt_up", 0); }
    s_cur = s; lv_screen_load(s_scr[s]);
}
ui_screen_t ui_current(void) { return s_cur; }

static void update_topbar(const ui_model_t *m)
{
    for (int i = 0; i < UI_SCREEN_COUNT; i++) {
        topbar_t *t = &s_top[i];
        lv_label_set_text(t->group, m->group);
        lv_label_set_text_fmt(t->batt, "%u%%", m->battery_pct);
        lv_label_set_text_fmt(t->link, "%s %d", LV_SYMBOL_WIFI, m->link_rssi_dbm);
        lv_obj_set_style_text_color(t->link, m->link_ok ? C_OK : C_TX, 0);
        const char *v = m->voice == UI_VOICE_TX ? "TX" : m->voice == UI_VOICE_RX ? "RX" : m->voice == UI_VOICE_BUSY ? "BUSY" : "";
        lv_label_set_text(t->voice, v);
        lv_obj_set_style_text_color(t->voice, m->voice == UI_VOICE_TX ? C_TX : m->voice == UI_VOICE_RX ? C_RX : lv_color_white(), 0);
    }
}

static const char *age_mark(pos_peer_age_t a) { return a == POS_PEER_FRESH ? "" : a == POS_PEER_DIM ? " ?" : " x"; }

void ui_update(const ui_model_t *m)
{
    if (!m) return;
    ui_model_t bounded;
    if (m->n_peers < 0 || m->n_peers > UI_MAX_PEERS) {
        bounded = *m;
        bounded.n_peers = m->n_peers < 0 ? 0 : UI_MAX_PEERS;
        m = &bounded;
    }
    update_topbar(m);
    // GROUP
    if (m->voice == UI_VOICE_RX) lv_label_set_text_fmt(s_group_big, "%s", m->talker);
    else if (m->voice == UI_VOICE_TX) lv_label_set_text(s_group_big, "YOU (TX)");
    else lv_label_set_text_fmt(s_group_big, "%d NODES", m->n_peers + 1);
    lv_obj_set_style_text_color(s_group_big, m->voice == UI_VOICE_TX ? C_TX : m->voice == UI_VOICE_RX ? C_RX : lv_color_white(), 0);
    for (int i = 0; i < UI_MAX_PEERS; i++) {
        if (i < m->n_peers) {
            lv_obj_remove_flag(s_group_rows[i], LV_OBJ_FLAG_HIDDEN);
            const ui_peer_t *p = &m->peers[i];
            lv_label_set_text_fmt(s_group_rows[i], "%s %-6s %4d dBm%s", p->talking ? LV_SYMBOL_VOLUME_MAX : p->muted ? LV_SYMBOL_MUTE : "  ", p->callsign, p->rssi_dbm, age_mark(p->age));
            lv_obj_set_style_text_color(s_group_rows[i], p->talking ? C_RX : p->age == POS_PEER_STALE ? C_DIM : lv_color_white(), 0);
        } else lv_obj_add_flag(s_group_rows[i], LV_OBJ_FLAG_HIDDEN);
    }
    // RADAR: heading-up, 3 rings; ring scale adapts to the farthest peer
    float far = 250; for (int i = 0; i < m->n_peers; i++) if (m->peers[i].age != POS_PEER_STALE && m->peers[i].dist_m > far) far = m->peers[i].dist_m;
    float ring = far <= 250 ? 250.0f : far <= 500 ? 500.0f : far <= 1000 ? 1000.0f : 2000.0f;   // outer ring = 84 px
    lv_label_set_text_fmt(s_radar_scale, "outer ring %.0f m", (double)ring);
    int label_x[UI_MAX_PEERS], label_y[UI_MAX_PEERS], label_count = 0;
    for (int i = 0; i < UI_MAX_PEERS; i++) {
        if (i < m->n_peers && m->peers[i].age != POS_PEER_STALE) {
            const ui_peer_t *p = &m->peers[i];
            float rel = (p->bearing_deg - m->heading_deg) * (float)M_PI / 180.0f;   // heading-up
            float r = fminf(p->dist_m / ring, 1.0f) * 84.0f;
            int x = 120 + (int)(sinf(rel) * r), y = 145 - (int)(cosf(rel) * r);
            lv_obj_set_pos(s_radar_dots[i], x - 7, y - 7); lv_obj_set_style_bg_color(s_radar_dots[i], p->talking ? C_RX : p->age == POS_PEER_DIM ? C_DIM : lv_color_white(), 0);
            lv_obj_clear_flag(s_radar_dots[i], LV_OBJ_FLAG_HIDDEN);
            if (p->age == POS_PEER_DIM) lv_label_set_text_fmt(s_radar_lbl[i], "%s ?", p->callsign);
            else lv_label_set_text_fmt(s_radar_lbl[i], "%s %.0fm", p->callsign, (double)p->dist_m);
            int lx = x + 9; if (lx + 100 > 236) lx = x - 109; if (lx < 4) lx = 4;
            int ly = y - 8; if (ly > 218) ly = 218; if (ly < 54) ly = 54;
            /* Find a free label lane; points stay at their geographic positions. */
            int preferred_y = ly;
            for (int offset = 0; offset < 10; offset++) {
                int candidate = preferred_y + offset * 18;
                if (candidate > 218) candidate = 54 + (candidate - 219);
                bool overlaps = false;
                for (int j = 0; j < label_count; j++) {
                    if (lx < label_x[j] + 100 && lx + 100 > label_x[j] &&
                        candidate < label_y[j] + 17 && candidate + 17 > label_y[j]) overlaps = true;
                }
                if (!overlaps) { ly = candidate; break; }
            }
            label_x[label_count] = lx; label_y[label_count++] = ly;
            lv_obj_set_pos(s_radar_lbl[i], lx, ly);
            lv_obj_clear_flag(s_radar_lbl[i], LV_OBJ_FLAG_HIDDEN);
        } else { lv_obj_add_flag(s_radar_dots[i], LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(s_radar_lbl[i], LV_OBJ_FLAG_HIDDEN); }
    }
    // CONVOY: sort by along_m descending (ahead first), insert YOU at 0
    int idx[UI_MAX_PEERS]; int n = m->n_peers; for (int i = 0; i < n; i++) idx[i] = i;
    for (int i = 1; i < n; i++) for (int j = i; j > 0 && m->peers[idx[j]].along_m > m->peers[idx[j - 1]].along_m; j--) { int t = idx[j]; idx[j] = idx[j - 1]; idx[j - 1] = t; }
    int row = 0; bool you_done = false; float maxa = -1e9f, mina = 1e9f;
    for (int k = 0; k < n && row <= UI_MAX_PEERS; k++) {
        const ui_peer_t *p = &m->peers[idx[k]];
        if (!you_done && p->along_m < 0) { lv_label_set_text(s_convoy_rows[row], "   YOU"); lv_obj_set_style_text_color(s_convoy_rows[row], C_OK, 0); row++; you_done = true; }
        if (row > UI_MAX_PEERS) break;
        if (p->age != POS_PEER_FRESH) lv_label_set_text_fmt(s_convoy_rows[row], "%-6s   ---%s", p->callsign, age_mark(p->age));
        else lv_label_set_text_fmt(s_convoy_rows[row], "%-6s %+6.0f m%s", p->callsign, (double)p->along_m, age_mark(p->age));
        lv_obj_set_style_text_color(s_convoy_rows[row], p->talking ? C_RX : p->age == POS_PEER_STALE ? C_DIM : lv_color_white(), 0);
        if (p->age == POS_PEER_FRESH) { if (p->along_m > maxa) maxa = p->along_m; if (p->along_m < mina) mina = p->along_m; }
        row++;
    }
    if (!you_done && row <= UI_MAX_PEERS) { lv_label_set_text(s_convoy_rows[row], "   YOU"); lv_obj_set_style_text_color(s_convoy_rows[row], C_OK, 0); row++; }
    for (int i = 0; i <= UI_MAX_PEERS; i++) {
        if (i < row) lv_obj_remove_flag(s_convoy_rows[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s_convoy_rows[i], LV_OBJ_FLAG_HIDDEN);
    }
    if (n > 0 && maxa > -1e8f) lv_label_set_text_fmt(s_convoy_spread, "spread %.0f m\n%.0f km/h   hdg %03.0f", (double)(fmaxf(maxa, 0) - fminf(mina, 0)), (double)m->speed_kmh, (double)m->heading_deg);
    else lv_label_set_text(s_convoy_spread, "no peers");
}
