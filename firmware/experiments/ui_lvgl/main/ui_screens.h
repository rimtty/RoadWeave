#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"
#include "position.h"

// Model the screens render from. Filled by the app (radio / GPS) or by the simulator.
#define UI_MAX_PEERS 8
typedef struct {
    uint32_t id; char callsign[8];
    int8_t rssi_dbm; bool talking; bool muted;
    float along_m;       // + ahead / - behind (convoy list), from position_along_heading
    float dist_m, bearing_deg;
    pos_peer_age_t age;
} ui_peer_t;

typedef enum { UI_VOICE_IDLE, UI_VOICE_RX, UI_VOICE_TX, UI_VOICE_BUSY } ui_voice_t;

typedef struct {
    char group[12];
    uint8_t battery_pct; int8_t link_rssi_dbm; bool link_ok;
    ui_voice_t voice; char talker[8];
    float heading_deg; float speed_kmh;
    ui_peer_t peers[UI_MAX_PEERS]; int n_peers;
} ui_model_t;

typedef enum { UI_SCREEN_GROUP = 0, UI_SCREEN_RADAR, UI_SCREEN_CONVOY, UI_SCREEN_COUNT } ui_screen_t;

void ui_create(lv_display_t *disp);            // builds all screens, shows GROUP
void ui_update(const ui_model_t *m);           // call from the LVGL-locked context, ~10 Hz
void ui_show(ui_screen_t s);
ui_screen_t ui_current(void);
// callbacks the app wires to radio actions (touch buttons): PTT press/release, mute toggle, screen cycle
typedef void (*ui_action_cb_t)(const char *action, uint32_t arg);
void ui_set_action_cb(ui_action_cb_t cb);
