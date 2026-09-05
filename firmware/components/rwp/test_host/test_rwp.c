#include "test.h"
#include "rwp.h"
#include <string.h>

static rwp_header_t sample(void)
{
    rwp_header_t h = {0};
    h.version = RWP_VERSION; h.type = RWP_TYPE_VOICE; h.codec = RWP_CODEC_IMA_ADPCM;
    h.flags = RWP_FLAG_START; h.group_id = 0xA1B2C3D4; h.sender_id = 7;
    h.target_type = RWP_TARGET_GROUP; h.target_id = 0; h.stream_id = 42;
    h.sequence = 0xFFFFFFFE; h.capture_time = 123456;
    return h;
}

static void test_roundtrip(void)
{
    uint8_t buf[256]; const uint8_t payload[] = {1, 2, 3, 4, 5};
    rwp_header_t h = sample();
    int n = rwp_encode(buf, sizeof buf, &h, payload, sizeof payload);
    CHECK_EQ(n, RWP_HEADER_LEN + 5);
    CHECK_EQ(buf[0], 0x52); CHECK_EQ(buf[1], 0x57);      // "RW" network order
    rwp_header_t d; const uint8_t *p = NULL;
    CHECK_EQ(rwp_decode(buf, (size_t)n, &d, &p), RWP_OK);
    CHECK(p == buf + RWP_HEADER_LEN);
    CHECK_EQ(d.type, RWP_TYPE_VOICE); CHECK_EQ(d.codec, RWP_CODEC_IMA_ADPCM);
    CHECK_EQ(d.flags, RWP_FLAG_START); CHECK_EQ(d.header_len, RWP_HEADER_LEN);
    CHECK_EQ(d.group_id, 0xA1B2C3D4); CHECK_EQ(d.sender_id, 7);
    CHECK_EQ(d.target_type, RWP_TARGET_GROUP); CHECK_EQ(d.target_id, 0);
    CHECK_EQ(d.stream_id, 42); CHECK_EQ(d.sequence, 0xFFFFFFFE);
    CHECK_EQ(d.capture_time, 123456); CHECK_EQ(d.payload_len, 5);
    CHECK(memcmp(p, payload, 5) == 0);
}

static void test_empty_payload_and_targets(void)
{
    uint8_t buf[64]; rwp_header_t h = sample(); h.type = RWP_TYPE_CONTROL; h.codec = 0;
    h.target_type = RWP_TARGET_NODE; h.target_id = 99;
    int n = rwp_encode(buf, sizeof buf, &h, NULL, 0);
    CHECK_EQ(n, RWP_HEADER_LEN);
    rwp_header_t d; const uint8_t *p;
    CHECK_EQ(rwp_decode(buf, (size_t)n, &d, &p), RWP_OK);
    CHECK_EQ(d.payload_len, 0); CHECK_EQ(d.target_id, 99);
    // GROUP with non-zero target_id is invalid
    h.target_type = RWP_TARGET_GROUP; h.target_id = 5;
    CHECK_EQ(rwp_encode(buf, sizeof buf, &h, NULL, 0), RWP_ERR_FIELD);
}

static void test_encode_errors(void)
{
    uint8_t buf[64]; rwp_header_t h = sample();
    CHECK_EQ(rwp_encode(NULL, 64, &h, NULL, 0), RWP_ERR_ARG);
    CHECK_EQ(rwp_encode(buf, RWP_HEADER_LEN - 1, &h, NULL, 0), RWP_ERR_ARG);
    CHECK_EQ(rwp_encode(buf, sizeof buf, &h, NULL, 10), RWP_ERR_ARG);        // payload NULL
    CHECK_EQ(rwp_encode(buf, sizeof buf, &h, buf, sizeof buf), RWP_ERR_ARG); // cap too small
    static uint8_t big[RWP_MAX_PAYLOAD + 1]; static uint8_t out[RWP_MAX_PAYLOAD + 64];
    CHECK_EQ(rwp_encode(out, sizeof out, &h, big, sizeof big), RWP_ERR_PAYLOAD_LEN);
    h = sample(); h.type = 9;  CHECK_EQ(rwp_encode(buf, sizeof buf, &h, NULL, 0), RWP_ERR_FIELD);
    h = sample(); h.codec = 9; CHECK_EQ(rwp_encode(buf, sizeof buf, &h, NULL, 0), RWP_ERR_FIELD);
    h = sample(); h.target_type = 9; CHECK_EQ(rwp_encode(buf, sizeof buf, &h, NULL, 0), RWP_ERR_FIELD);
}

static void test_decode_errors(void)
{
    uint8_t buf[128]; rwp_header_t h = sample(); uint8_t pl[8] = {0};
    int n = rwp_encode(buf, sizeof buf, &h, pl, sizeof pl);
    rwp_header_t d; const uint8_t *p;
    // truncation at every length must fail cleanly, never succeed
    for (int len = 0; len < n; len++) CHECK(rwp_decode(buf, (size_t)len, &d, &p) != RWP_OK);
    uint8_t t[128];
    memcpy(t, buf, (size_t)n); t[0] = 0; CHECK_EQ(rwp_decode(t, (size_t)n, &d, &p), RWP_ERR_MAGIC);
    memcpy(t, buf, (size_t)n); t[2] = 2; CHECK_EQ(rwp_decode(t, (size_t)n, &d, &p), RWP_ERR_VERSION);
    memcpy(t, buf, (size_t)n); t[7] = 0; t[8] = 10; CHECK_EQ(rwp_decode(t, (size_t)n, &d, &p), RWP_ERR_HEADER_LEN);
    memcpy(t, buf, (size_t)n); t[7] = 0xFF; t[8] = 0xFF; CHECK_EQ(rwp_decode(t, (size_t)n, &d, &p), RWP_ERR_HEADER_LEN);
    memcpy(t, buf, (size_t)n); t[34] = 0x10; t[35] = 0; CHECK_EQ(rwp_decode(t, (size_t)n, &d, &p), RWP_ERR_PAYLOAD_LEN);
    memcpy(t, buf, (size_t)n); t[3] = 0x77; CHECK_EQ(rwp_decode(t, (size_t)n, &d, &p), RWP_ERR_FIELD);
    // extended header: header_len 40, decoder must skip the extra 4 bytes
    memcpy(t, buf, RWP_HEADER_LEN); t[7] = 0; t[8] = 40; memset(t + 36, 0xEE, 4); memcpy(t + 40, pl, 8);
    CHECK_EQ(rwp_decode(t, 48, &d, &p), RWP_OK); CHECK(p == t + 40); CHECK_EQ(d.header_len, 40);
    // unknown flag bits are preserved
    memcpy(t, buf, (size_t)n); t[5] = 0x80; t[6] = 0x00;
    CHECK_EQ(rwp_decode(t, (size_t)n, &d, &p), RWP_OK); CHECK_EQ(d.flags, 0x8000);
}

static void test_random_garbage_never_crashes(void)
{
    uint32_t x = 12345; uint8_t buf[200]; rwp_header_t d; const uint8_t *p; int ok = 0;
    for (int i = 0; i < 20000; i++) {
        size_t len = (size_t)(x % sizeof buf); x = x * 1664525u + 1013904223u;
        for (size_t j = 0; j < len; j++) { buf[j] = (uint8_t)(x >> 24); x = x * 1664525u + 1013904223u; }
        int r = rwp_decode(buf, len, &d, &p);
        if (r == RWP_OK) { ok++; CHECK(p != NULL && p + d.payload_len <= buf + len); }
    }
    CHECK(ok >= 0);  // just exercise; no crash / no out-of-range payload pointer
}

static void test_seq(void)
{
    CHECK(rwp_seq_after(1, 0)); CHECK(!rwp_seq_after(0, 1)); CHECK(!rwp_seq_after(5, 5));
    CHECK(rwp_seq_after(0, 0xFFFFFFFF));            // wrap
    CHECK(!rwp_seq_after(0xFFFFFFFF, 0));
    CHECK(rwp_seq_after(0x80000000, 1)); CHECK(!rwp_seq_after(0x80000001, 1));
}

static void test_control(void)
{
    uint8_t b[16]; rwp_control_t c = { .ctrl = RWP_CTRL_FLOOR_GRANT, .stream_id = 42, .lease_ms = 750 }, d;
    CHECK_EQ(rwp_control_encode(b, sizeof b, &c), RWP_CONTROL_LEN);
    CHECK_EQ(rwp_control_decode(b, RWP_CONTROL_LEN, &d), RWP_OK);
    CHECK_EQ(d.ctrl, RWP_CTRL_FLOOR_GRANT); CHECK_EQ(d.stream_id, 42); CHECK_EQ(d.lease_ms, 750);
    CHECK_EQ(rwp_control_decode(b, RWP_CONTROL_LEN - 1, &d), RWP_ERR_SHORT);
    b[0] = 0;  CHECK_EQ(rwp_control_decode(b, RWP_CONTROL_LEN, &d), RWP_ERR_FIELD);
    b[0] = 99; CHECK_EQ(rwp_control_decode(b, RWP_CONTROL_LEN, &d), RWP_ERR_FIELD);
    c.ctrl = 0; CHECK_EQ(rwp_control_encode(b, sizeof b, &c), RWP_ERR_FIELD);
    c.ctrl = RWP_CTRL_PTT_END; CHECK_EQ(rwp_control_encode(b, 4, &c), RWP_ERR_ARG);
}

int main(void)
{
    test_roundtrip(); test_empty_payload_and_targets(); test_encode_errors();
    test_decode_errors(); test_random_garbage_never_crashes(); test_seq(); test_control();
    printf("test_rwp: %d checks, %d failures\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
