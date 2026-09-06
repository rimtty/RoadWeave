#include <math.h>
#include <string.h>
#include "ui_simulation.h"

// --- simulator: 3 cars on a gentle curve, self in the middle ---
void ui_simulate(ui_model_t *m, float t)
{
    memset(m, 0, sizeof(*m));
    strcpy(m->group, "GROUP A"); m->battery_pct = 87; m->link_rssi_dbm = -64; m->link_ok = true;
    m->heading_deg = fmodf(45 + 20 * sinf(t / 30), 360); m->speed_kmh = 62 + 8 * sinf(t / 7);
    pos_local_t self = { 0, 0 };
    const char *names[] = { "LOTUS", "NA", "ND" }; const int8_t rssi[] = { -64, -72, -84 };
    float along[] = { 120 + 30 * sinf(t / 11), -340 - 60 * sinf(t / 9), -980 - 100 * sinf(t / 13) };
    m->n_peers = 3;
    for (int i = 0; i < 3; i++) {
        ui_peer_t *p = &m->peers[i]; p->id = 0x101 + i; strcpy(p->callsign, names[i]); p->rssi_dbm = rssi[i]; p->muted = false;
        float h = m->heading_deg * (float)M_PI / 180; float side = 15 * sinf(t / 5 + i);
        pos_local_t pp = { self.east_m + along[i] * sinf(h) + side * cosf(h), self.north_m + along[i] * cosf(h) - side * sinf(h) };
        p->dist_m = pos_distance_m(&self, &pp); p->bearing_deg = pos_bearing_deg(&self, &pp);
        p->along_m = pos_along_heading_m(&self, m->heading_deg, &pp);
        p->age = (i == 2 && fmodf(t, 40) > 30) ? POS_PEER_DIM : POS_PEER_FRESH;
        p->talking = false;
    }
    int phase = (int)fmodf(t, 24);
    m->voice = phase < 6 ? UI_VOICE_RX : phase < 8 ? UI_VOICE_IDLE : phase < 12 ? UI_VOICE_TX : UI_VOICE_IDLE;
    if (m->voice == UI_VOICE_RX) { m->peers[0].talking = true; strcpy(m->talker, "LOTUS"); }
}
