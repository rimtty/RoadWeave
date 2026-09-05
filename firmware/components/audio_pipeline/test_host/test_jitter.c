#include "test.h"
#include "jitter.h"
#include <string.h>

static uint8_t pl(uint8_t tag) { return tag; }

static void put(jb_t *jb, uint32_t seq, uint32_t now, jb_put_result_t expect)
{
    uint8_t p[4] = { pl((uint8_t)seq), 1, 2, 3 };
    CHECK_EQ(jb_put(jb, 1, seq, p, sizeof p, now), expect);
}

static void test_prefill_and_in_order(void)
{
    jb_t jb; jb_init(&jb, NULL);           // target 40 ms = 2 frames
    uint8_t out[JB_MAX_PAYLOAD]; size_t len; uint32_t t = 0;
    CHECK_EQ(jb_get(&jb, out, sizeof out, &len, t), JB_GET_WAIT);
    put(&jb, 10, t, JB_PUT_OK);
    CHECK_EQ(jb_get(&jb, out, sizeof out, &len, t += 20), JB_GET_WAIT);   // 1 < 2 frames
    put(&jb, 11, t, JB_PUT_OK);
    CHECK_EQ(jb_get(&jb, out, sizeof out, &len, t += 20), JB_GET_FRAME); CHECK_EQ(out[0], 10); CHECK_EQ(len, 4);
    put(&jb, 12, t, JB_PUT_OK);
    CHECK_EQ(jb_get(&jb, out, sizeof out, &len, t += 20), JB_GET_FRAME); CHECK_EQ(out[0], 11);
    CHECK_EQ(jb_get(&jb, out, sizeof out, &len, t += 20), JB_GET_FRAME); CHECK_EQ(out[0], 12);
    CHECK_EQ(jb_get(&jb, out, sizeof out, &len, t += 20), JB_GET_UNDERRUN);
    CHECK_EQ(jb_stats(&jb)->frames_played, 3); CHECK_EQ(jb_stats(&jb)->underrun, 1);
}

static void test_reorder_gap_late_duplicate(void)
{
    jb_t jb; jb_init(&jb, NULL); uint8_t out[JB_MAX_PAYLOAD]; size_t len; uint32_t t = 0;
    put(&jb, 1, t, JB_PUT_OK); put(&jb, 3, t, JB_PUT_OK);      // 2 missing so far
    put(&jb, 2, t, JB_PUT_OK);                                  // reordered arrival
    put(&jb, 2, t, JB_PUT_DUPLICATE);
    CHECK_EQ(jb_get(&jb, out, sizeof out, &len, t += 20), JB_GET_FRAME); CHECK_EQ(out[0], 1);
    CHECK_EQ(jb_get(&jb, out, sizeof out, &len, t += 20), JB_GET_FRAME); CHECK_EQ(out[0], 2);
    put(&jb, 5, t, JB_PUT_OK);                                  // 4 lost
    CHECK_EQ(jb_get(&jb, out, sizeof out, &len, t += 20), JB_GET_FRAME); CHECK_EQ(out[0], 3);
    CHECK_EQ(jb_get(&jb, out, sizeof out, &len, t += 20), JB_GET_GAP);     // 4
    put(&jb, 1, t, JB_PUT_LATE);                                // way too old
    CHECK_EQ(jb_get(&jb, out, sizeof out, &len, t += 20), JB_GET_FRAME); CHECK_EQ(out[0], 5);
    CHECK_EQ(jb_stats(&jb)->gap, 1); CHECK_EQ(jb_stats(&jb)->late, 1); CHECK_EQ(jb_stats(&jb)->duplicate, 1);
}

static void test_too_far_resets(void)
{
    jb_t jb; jb_init(&jb, NULL); uint8_t out[JB_MAX_PAYLOAD]; size_t len;
    put(&jb, 100, 0, JB_PUT_OK); put(&jb, 101, 0, JB_PUT_OK);
    put(&jb, 100 + JB_MAX_FRAMES + 5, 0, JB_PUT_OK);            // burst loss: restart from here
    CHECK_EQ(jb_stats(&jb)->too_far, 1); CHECK_EQ(jb.head_seq, 100 + JB_MAX_FRAMES + 5);
    CHECK_EQ(jb_buffered(&jb), 1);
    CHECK_EQ(jb_get(&jb, out, sizeof out, &len, 20), JB_GET_WAIT);  // prefill again
}

static void test_new_stream_flushes(void)
{
    jb_t jb; jb_init(&jb, NULL); uint8_t p[2] = {9, 9}; uint8_t out[JB_MAX_PAYLOAD]; size_t len;
    CHECK_EQ(jb_put(&jb, 1, 50, p, 2, 0), JB_PUT_OK); CHECK_EQ(jb_put(&jb, 1, 51, p, 2, 0), JB_PUT_OK);
    CHECK_EQ(jb_put(&jb, 2, 7, p, 2, 0), JB_PUT_OK);            // stream switch: old frames gone
    CHECK_EQ(jb.stream_id, 2); CHECK_EQ(jb.head_seq, 7); CHECK_EQ(jb_buffered(&jb), 1);
    CHECK_EQ(jb_get(&jb, out, sizeof out, &len, 20), JB_GET_WAIT);
}

static void test_adaptive_grow_on_late(void)
{
    jb_t jb; jb_init(&jb, NULL); uint8_t out[JB_MAX_PAYLOAD]; size_t len; uint32_t t = 0;
    CHECK_EQ(jb_stats(&jb)->depth_ms, 40);
    put(&jb, 1, t, JB_PUT_OK); put(&jb, 2, t, JB_PUT_OK); put(&jb, 3, t, JB_PUT_OK);
    jb_get(&jb, out, sizeof out, &len, t += 20); jb_get(&jb, out, sizeof out, &len, t += 20);
    put(&jb, 1, t, JB_PUT_LATE); put(&jb, 2, t, JB_PUT_LATE);
    CHECK_EQ(jb_stats(&jb)->depth_ms, 40);
    put(&jb, 1, t, JB_PUT_LATE);                                 // 3rd late in window -> grow
    CHECK_EQ(jb_stats(&jb)->depth_ms, 60); CHECK_EQ(jb_stats(&jb)->grow, 1);
    for (int i = 0; i < 20; i++) put(&jb, 1, t, JB_PUT_LATE);   // capped at max_ms
    CHECK_EQ(jb_stats(&jb)->depth_ms, 80);
}

static void test_adaptive_shrink_on_sustained_overdepth(void)
{
    jb_cfg_t c = jb_cfg_default(); c.target_ms = 60; c.shrink_after_ms = 100;
    jb_t jb; jb_init(&jb, &c); uint8_t out[JB_MAX_PAYLOAD]; size_t len; uint32_t t = 1000, seq = 1;
    for (int i = 0; i < 6; i++) put(&jb, seq++, t, JB_PUT_OK);   // 6 buffered, target 3
    int frames = 0, shr = 0;
    for (int i = 0; i < 12; i++) {
        put(&jb, seq++, t, JB_PUT_OK);                           // keep depth high
        jb_get_result_t r = jb_get(&jb, out, sizeof out, &len, t += 20);
        if (r == JB_GET_FRAME) frames++;
        shr = (int)jb_stats(&jb)->shrink;
    }
    CHECK(shr >= 1); CHECK(jb_stats(&jb)->depth_ms < 60); CHECK(jb_stats(&jb)->depth_ms >= 20); CHECK(frames > 0);
}

static void test_seq_wrap(void)
{
    jb_t jb; jb_init(&jb, NULL); uint8_t out[JB_MAX_PAYLOAD]; size_t len; uint32_t t = 0;
    put(&jb, 0xFFFFFFFEu, t, JB_PUT_OK); put(&jb, 0xFFFFFFFFu, t, JB_PUT_OK); put(&jb, 0, t, JB_PUT_OK);
    CHECK_EQ(jb_get(&jb, out, sizeof out, &len, t += 20), JB_GET_FRAME); CHECK_EQ(out[0], 0xFE);
    CHECK_EQ(jb_get(&jb, out, sizeof out, &len, t += 20), JB_GET_FRAME); CHECK_EQ(out[0], 0xFF);
    CHECK_EQ(jb_get(&jb, out, sizeof out, &len, t += 20), JB_GET_FRAME); CHECK_EQ(out[0], 0);
}

static void test_bad_args(void)
{
    jb_t jb; jb_init(&jb, NULL); uint8_t big[JB_MAX_PAYLOAD + 1] = {0};
    CHECK_EQ(jb_put(&jb, 1, 1, big, sizeof big, 0), JB_PUT_ERR);
    CHECK_EQ(jb_put(&jb, 1, 1, big, 0, 0), JB_PUT_ERR);
    CHECK_EQ(jb_put(&jb, 1, 1, NULL, 4, 0), JB_PUT_ERR);
}

int main(void)
{
    test_prefill_and_in_order(); test_reorder_gap_late_duplicate(); test_too_far_resets();
    test_new_stream_flushes(); test_adaptive_grow_on_late(); test_adaptive_shrink_on_sustained_overdepth();
    test_seq_wrap(); test_bad_args();
    printf("test_jitter: %d checks, %d failures\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
