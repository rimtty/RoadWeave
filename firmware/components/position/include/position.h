// Position beacon (docs/gps-and-maps.md §2), peer table and local ENU math for the
// convoy / radar views. Pure C11, no ESP-IDF, float math only (short-range use).
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define POS_BEACON_LEN 28u   // wire size of pos_beacon_t (group/sender live in the RWP header)

typedef enum { POS_FIX_NONE = 0, POS_FIX_2D = 1, POS_FIX_3D = 2, POS_FIX_DGPS = 3 } pos_fix_t;

typedef struct {
    uint64_t gnss_utc_ms;   // 0 if unknown
    int32_t  lat_e7;        // degrees * 1e7
    int32_t  lon_e7;
    int32_t  alt_dm;        // decimetres, INT32_MIN if unknown
    uint16_t speed_cms;     // cm/s
    uint16_t heading_cdeg;  // 0..35999 (centi-degrees, true north); 65535 = unknown
    uint16_t hacc_cm;       // horizontal accuracy, 65535 = unknown
    uint8_t  fix_type;      // pos_fix_t
    uint8_t  sats;
} pos_beacon_t;

#define POS_HEADING_UNKNOWN 65535u
#define POS_HACC_UNKNOWN    65535u

int  pos_beacon_encode(uint8_t *out, size_t cap, const pos_beacon_t *b);            // bytes or <0
int  pos_beacon_decode(const uint8_t *in, size_t len, pos_beacon_t *b);            // 0 or <0
bool pos_beacon_valid(const pos_beacon_t *b);   // fix present and coordinates in range (never draw (0,0))

// ---- local ENU frame ----
typedef struct { double lat0_rad, lon0_rad, cos_lat0; } pos_frame_t;
typedef struct { float east_m, north_m; } pos_local_t;

void  pos_frame_init(pos_frame_t *f, int32_t lat_e7, int32_t lon_e7);
void  pos_to_local(const pos_frame_t *f, int32_t lat_e7, int32_t lon_e7, pos_local_t *out);
float pos_distance_m(const pos_local_t *a, const pos_local_t *b);
float pos_bearing_deg(const pos_local_t *from, const pos_local_t *to);   // 0..360, 0 = north, 90 = east
// Signed distance of `peer` along `heading_deg` from `self`: positive = ahead, negative = behind.
float pos_along_heading_m(const pos_local_t *self, float heading_deg, const pos_local_t *peer);
// Rebase is needed when far from origin (float precision / flat-earth error). ~20 km is safe.
bool  pos_frame_needs_rebase(const pos_local_t *p, float limit_m);

// ---- peer table ----
#define POS_MAX_PEERS 16
#define POS_AGE_DIM_MS   3000
#define POS_AGE_STALE_MS 10000

typedef enum { POS_PEER_FRESH = 0, POS_PEER_DIM, POS_PEER_STALE, POS_PEER_NONE } pos_peer_age_t;

typedef struct {
    bool     used;
    uint32_t sender_id;
    uint32_t last_seq;
    uint32_t last_rx_ms;
    pos_beacon_t beacon;
} pos_peer_t;

typedef struct { pos_peer_t peers[POS_MAX_PEERS]; } pos_table_t;

void pos_table_init(pos_table_t *t);
// Returns the peer slot or NULL if the table is full. Older sequence numbers are ignored (newest wins).
pos_peer_t *pos_table_update(pos_table_t *t, uint32_t sender_id, uint32_t seq, const pos_beacon_t *b, uint32_t now_ms);
pos_peer_t *pos_table_find(pos_table_t *t, uint32_t sender_id);
pos_peer_age_t pos_peer_age(const pos_peer_t *p, uint32_t now_ms);
// Remove peers not heard from for `expire_ms`.
void pos_table_expire(pos_table_t *t, uint32_t now_ms, uint32_t expire_ms);

#ifdef __cplusplus
}
#endif
