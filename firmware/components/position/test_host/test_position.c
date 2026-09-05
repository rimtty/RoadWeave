#include "test.h"
#include "position.h"
#include <math.h>
#include <string.h>

static void test_beacon_roundtrip(void)
{
    pos_beacon_t b = { .gnss_utc_ms = 1757100000123ull, .lat_e7 = 356812345, .lon_e7 = 1397671234, .alt_dm = 412,
                       .speed_cms = 2778, .heading_cdeg = 12345, .hacc_cm = 250, .fix_type = POS_FIX_3D, .sats = 11 };
    uint8_t buf[POS_BEACON_LEN]; pos_beacon_t d;
    CHECK_EQ(pos_beacon_encode(buf, sizeof buf, &b), POS_BEACON_LEN);
    CHECK_EQ(pos_beacon_decode(buf, sizeof buf, &d), 0);
    CHECK(memcmp(&b, &d, sizeof b) == 0);
    CHECK_EQ(pos_beacon_encode(buf, POS_BEACON_LEN - 1, &b), -1);
    CHECK_EQ(pos_beacon_decode(buf, POS_BEACON_LEN - 1, &d), -1);
    b.heading_cdeg = 36000; CHECK_EQ(pos_beacon_encode(buf, sizeof buf, &b), -2);
    b.heading_cdeg = POS_HEADING_UNKNOWN; CHECK_EQ(pos_beacon_encode(buf, sizeof buf, &b), POS_BEACON_LEN);
    buf[26] = 9; CHECK_EQ(pos_beacon_decode(buf, sizeof buf, &d), -2);
    // negative coordinates survive (southern / western hemisphere)
    pos_beacon_t s = b; s.lat_e7 = -338688000; s.lon_e7 = -1512093000; s.alt_dm = -5;
    pos_beacon_encode(buf, sizeof buf, &s); pos_beacon_decode(buf, sizeof buf, &d);
    CHECK_EQ(d.lat_e7, -338688000); CHECK_EQ(d.lon_e7, -1512093000); CHECK_EQ(d.alt_dm, -5);
}

static void test_valid(void)
{
    pos_beacon_t b = { .lat_e7 = 356812345, .lon_e7 = 1397671234, .fix_type = POS_FIX_3D };
    CHECK(pos_beacon_valid(&b));
    b.fix_type = POS_FIX_NONE; CHECK(!pos_beacon_valid(&b));
    b.fix_type = POS_FIX_2D; b.lat_e7 = 0; b.lon_e7 = 0; CHECK(!pos_beacon_valid(&b));   // never draw (0,0)
    b.lat_e7 = 900000001; b.lon_e7 = 1; CHECK(!pos_beacon_valid(&b));
}

static void test_local_frame(void)
{
    pos_frame_t f; pos_frame_init(&f, 356800000, 1397700000);        // Tokyo-ish
    pos_local_t p;
    pos_to_local(&f, 356800000 + 100000, 1397700000, &p);           // +0.01 deg lat = ~1112 m north
    CHECK(fabsf(p.north_m - 1111.95f) < 1.0f); CHECK(fabsf(p.east_m) < 0.01f);
    pos_to_local(&f, 356800000, 1397700000 + 100000, &p);           // +0.01 deg lon at 35.68N = ~903 m east
    CHECK(fabsf(p.east_m - 903.3f) < 1.5f); CHECK(fabsf(p.north_m) < 0.01f);
    pos_local_t o = {0, 0}; pos_local_t n = {0, 100}, e = {100, 0}, s = {0, -100}, w = {-100, 0}, ne = {100, 100};
    CHECK(fabsf(pos_bearing_deg(&o, &n) - 0) < 0.01f); CHECK(fabsf(pos_bearing_deg(&o, &e) - 90) < 0.01f);
    CHECK(fabsf(pos_bearing_deg(&o, &s) - 180) < 0.01f); CHECK(fabsf(pos_bearing_deg(&o, &w) - 270) < 0.01f);
    CHECK(fabsf(pos_bearing_deg(&o, &ne) - 45) < 0.01f);
    CHECK(fabsf(pos_distance_m(&o, &ne) - 141.42f) < 0.01f);
    // convoy: heading north, peer 120 m north is +120 (ahead); peer 620 m south is -620 (behind); peer east is 0
    CHECK(fabsf(pos_along_heading_m(&o, 0, &n) - 100) < 0.01f);
    CHECK(fabsf(pos_along_heading_m(&o, 0, &s) + 100) < 0.01f);
    CHECK(fabsf(pos_along_heading_m(&o, 0, &e)) < 0.01f);
    CHECK(fabsf(pos_along_heading_m(&o, 90, &e) - 100) < 0.01f);   // heading east: east peer is ahead
    // rebase check and antimeridian wrap
    pos_local_t far = {25000, 0}; CHECK(pos_frame_needs_rebase(&far, 20000)); CHECK(!pos_frame_needs_rebase(&o, 20000));
    pos_frame_t g; pos_frame_init(&g, 0, 1799990000); pos_to_local(&g, 0, -1799990000, &p);   // across 180E/W: ~222 m east, not 40,000 km
    CHECK(fabsf(p.east_m - 222.4f) < 1.0f);
}

static void test_peer_table(void)
{
    pos_table_t t; pos_table_init(&t);
    pos_beacon_t b = { .lat_e7 = 1, .lon_e7 = 2, .fix_type = POS_FIX_3D };
    pos_peer_t *p = pos_table_update(&t, 0x101, 10, &b, 1000);
    CHECK(p && p->last_seq == 10);
    b.lat_e7 = 5; pos_table_update(&t, 0x101, 9, &b, 1100);           // older seq ignored
    CHECK_EQ(pos_table_find(&t, 0x101)->beacon.lat_e7, 1); CHECK_EQ(pos_table_find(&t, 0x101)->last_rx_ms, 1000);
    pos_table_update(&t, 0x101, 11, &b, 1200);
    CHECK_EQ(pos_table_find(&t, 0x101)->beacon.lat_e7, 5);
    CHECK_EQ(pos_peer_age(pos_table_find(&t, 0x101), 1200), POS_PEER_FRESH);
    CHECK_EQ(pos_peer_age(pos_table_find(&t, 0x101), 1200 + 3000), POS_PEER_DIM);
    CHECK_EQ(pos_peer_age(pos_table_find(&t, 0x101), 1200 + 10000), POS_PEER_STALE);
    for (uint32_t i = 0; i < POS_MAX_PEERS - 1; i++) CHECK(pos_table_update(&t, 0x200 + i, 1, &b, 1300) != NULL);
    CHECK(pos_table_update(&t, 0x999, 1, &b, 1300) == NULL);           // full
    pos_table_expire(&t, 1300 + 30000, 30000);
    CHECK(pos_table_find(&t, 0x101) == NULL); CHECK(pos_table_update(&t, 0x999, 1, &b, 40000) != NULL);
    // wrap-safe age
    pos_peer_t w = { .used = true, .last_rx_ms = 0xFFFFFF00u }; CHECK_EQ(pos_peer_age(&w, 0x00000100u), POS_PEER_FRESH);
}

int main(void)
{
    test_beacon_roundtrip(); test_valid(); test_local_frame(); test_peer_table();
    printf("test_position: %d checks, %d failures\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
