#include "position.h"
#include <math.h>
#include <string.h>

#define EARTH_R_M 6371008.8
#define DEG2RAD (M_PI / 180.0)

static void put_u16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
static void put_u32(uint8_t *p, uint32_t v) { put_u16(p, (uint16_t)(v >> 16)); put_u16(p + 2, (uint16_t)v); }
static void put_u64(uint8_t *p, uint64_t v) { put_u32(p, (uint32_t)(v >> 32)); put_u32(p + 4, (uint32_t)v); }
static uint16_t get_u16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }
static uint32_t get_u32(const uint8_t *p) { return ((uint32_t)get_u16(p) << 16) | get_u16(p + 2); }
static uint64_t get_u64(const uint8_t *p) { return ((uint64_t)get_u32(p) << 32) | get_u32(p + 4); }

int pos_beacon_encode(uint8_t *out, size_t cap, const pos_beacon_t *b)
{
    if (!out || !b || cap < POS_BEACON_LEN) return -1;
    if (b->heading_cdeg != POS_HEADING_UNKNOWN && b->heading_cdeg >= 36000) return -2;
    if (b->fix_type > POS_FIX_DGPS) return -2;
    put_u64(out, b->gnss_utc_ms);
    put_u32(out + 8, (uint32_t)b->lat_e7);
    put_u32(out + 12, (uint32_t)b->lon_e7);
    put_u32(out + 16, (uint32_t)b->alt_dm);
    put_u16(out + 20, b->speed_cms);
    put_u16(out + 22, b->heading_cdeg);
    put_u16(out + 24, b->hacc_cm);
    out[26] = b->fix_type; out[27] = b->sats;
    return (int)POS_BEACON_LEN;
}

int pos_beacon_decode(const uint8_t *in, size_t len, pos_beacon_t *b)
{
    if (!in || !b || len < POS_BEACON_LEN) return -1;
    pos_beacon_t t;
    t.gnss_utc_ms = get_u64(in);
    t.lat_e7 = (int32_t)get_u32(in + 8);
    t.lon_e7 = (int32_t)get_u32(in + 12);
    t.alt_dm = (int32_t)get_u32(in + 16);
    t.speed_cms = get_u16(in + 20);
    t.heading_cdeg = get_u16(in + 22);
    t.hacc_cm = get_u16(in + 24);
    t.fix_type = in[26]; t.sats = in[27];
    if (t.fix_type > POS_FIX_DGPS) return -2;
    if (t.heading_cdeg != POS_HEADING_UNKNOWN && t.heading_cdeg >= 36000) return -2;
    *b = t;
    return 0;
}

bool pos_beacon_valid(const pos_beacon_t *b)
{
    if (!b || b->fix_type == POS_FIX_NONE) return false;
    if (b->lat_e7 < -900000000 || b->lat_e7 > 900000000) return false;
    if (b->lon_e7 < -1800000000 || b->lon_e7 > 1800000000) return false;
    if (b->lat_e7 == 0 && b->lon_e7 == 0) return false;   // classic "no fix" coordinate
    return true;
}

void pos_frame_init(pos_frame_t *f, int32_t lat_e7, int32_t lon_e7)
{
    f->lat0_rad = lat_e7 * 1e-7 * DEG2RAD;
    f->lon0_rad = lon_e7 * 1e-7 * DEG2RAD;
    f->cos_lat0 = cos(f->lat0_rad);
}

void pos_to_local(const pos_frame_t *f, int32_t lat_e7, int32_t lon_e7, pos_local_t *out)
{
    double lat = lat_e7 * 1e-7 * DEG2RAD, lon = lon_e7 * 1e-7 * DEG2RAD;
    double dlon = lon - f->lon0_rad;
    if (dlon > M_PI) dlon -= 2 * M_PI;
    if (dlon < -M_PI) dlon += 2 * M_PI;
    out->east_m  = (float)(dlon * f->cos_lat0 * EARTH_R_M);
    out->north_m = (float)((lat - f->lat0_rad) * EARTH_R_M);
}

float pos_distance_m(const pos_local_t *a, const pos_local_t *b)
{
    float de = b->east_m - a->east_m, dn = b->north_m - a->north_m;
    return sqrtf(de * de + dn * dn);
}

float pos_bearing_deg(const pos_local_t *from, const pos_local_t *to)
{
    float de = to->east_m - from->east_m, dn = to->north_m - from->north_m;
    float deg = (float)(atan2((double)de, (double)dn) / DEG2RAD);
    if (deg < 0) deg += 360.0f;
    if (deg >= 360.0f) deg -= 360.0f;
    return deg;
}

float pos_along_heading_m(const pos_local_t *self, float heading_deg, const pos_local_t *peer)
{
    float h = heading_deg * (float)DEG2RAD;
    float ux = sinf(h), uy = cosf(h);     // unit vector along heading (east, north)
    return (peer->east_m - self->east_m) * ux + (peer->north_m - self->north_m) * uy;
}

bool pos_frame_needs_rebase(const pos_local_t *p, float limit_m)
{
    return fabsf(p->east_m) > limit_m || fabsf(p->north_m) > limit_m;
}

void pos_table_init(pos_table_t *t) { memset(t, 0, sizeof *t); }

pos_peer_t *pos_table_find(pos_table_t *t, uint32_t sender_id)
{
    for (int i = 0; i < POS_MAX_PEERS; i++) if (t->peers[i].used && t->peers[i].sender_id == sender_id) return &t->peers[i];
    return NULL;
}

pos_peer_t *pos_table_update(pos_table_t *t, uint32_t sender_id, uint32_t seq, const pos_beacon_t *b, uint32_t now)
{
    pos_peer_t *p = pos_table_find(t, sender_id);
    if (!p) {
        for (int i = 0; i < POS_MAX_PEERS && !p; i++) if (!t->peers[i].used) p = &t->peers[i];
        if (!p) return NULL;
        memset(p, 0, sizeof *p); p->used = true; p->sender_id = sender_id; p->last_seq = seq - 1;
    } else if (seq != p->last_seq && (uint32_t)(seq - p->last_seq) >= 0x80000000u) {
        return p;   // older than what we have (reordered/duplicate): keep newest
    }
    p->last_seq = seq; p->last_rx_ms = now; p->beacon = *b;
    return p;
}

pos_peer_age_t pos_peer_age(const pos_peer_t *p, uint32_t now)
{
    if (!p || !p->used) return POS_PEER_NONE;
    uint32_t age = now - p->last_rx_ms;
    if (age >= POS_AGE_STALE_MS) return POS_PEER_STALE;
    if (age >= POS_AGE_DIM_MS) return POS_PEER_DIM;
    return POS_PEER_FRESH;
}

void pos_table_expire(pos_table_t *t, uint32_t now, uint32_t expire_ms)
{
    for (int i = 0; i < POS_MAX_PEERS; i++) if (t->peers[i].used && (uint32_t)(now - t->peers[i].last_rx_ms) >= expire_ms) t->peers[i].used = false;
}
