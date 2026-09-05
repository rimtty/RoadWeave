#include "test.h"
#include "floor.h"

static floor_node_t node(void) { floor_node_t n; floor_node_init(&n, NULL, 100); return n; }

static void test_happy_path(void)
{
    floor_node_t n = node(); uint32_t t = 1000, a;
    a = floor_node_step(&n, FLOOR_EV_PTT_DOWN, t);
    CHECK_EQ(a, FLOOR_ACT_SEND_REQUEST); CHECK_EQ(n.state, FLOOR_REQUESTING); CHECK_EQ(n.stream_id, 100);
    a = floor_node_step(&n, FLOOR_EV_GRANT, t + 30);
    CHECK(a & FLOOR_ACT_START_TX); CHECK(a & FLOOR_ACT_INDICATE_GRANT); CHECK_EQ(n.state, FLOOR_TALKING);
    // voice packets keep the lease fresh: no RENEW while voice flows
    for (int i = 1; i <= 10; i++) {
        floor_node_step(&n, FLOOR_EV_VOICE_SENT, t + 30 + 20u * i);
        a = floor_node_step(&n, FLOOR_EV_TICK, t + 30 + 20u * i + 5);
        CHECK_EQ(a, 0);
    }
    // silence during PTT: RENEW after renew_interval
    a = floor_node_step(&n, FLOOR_EV_TICK, t + 230 + 250);
    CHECK_EQ(a, FLOOR_ACT_SEND_RENEW);
    a = floor_node_step(&n, FLOOR_EV_TICK, t + 230 + 250 + 10);
    CHECK_EQ(a, 0);
    // release: STOP_TX immediately + END x3 spaced by end_interval
    uint32_t t_up = t + 1000;
    a = floor_node_step(&n, FLOOR_EV_PTT_UP, t_up);
    CHECK_EQ(a, FLOOR_ACT_STOP_TX | FLOOR_ACT_SEND_END); CHECK_EQ(n.state, FLOOR_ENDING);
    CHECK_EQ(floor_node_step(&n, FLOOR_EV_TICK, t_up + 10), 0);
    CHECK_EQ(floor_node_step(&n, FLOOR_EV_TICK, t_up + 50), FLOOR_ACT_SEND_END);
    CHECK_EQ(floor_node_step(&n, FLOOR_EV_TICK, t_up + 100), FLOOR_ACT_SEND_END);
    CHECK_EQ(floor_node_step(&n, FLOOR_EV_TICK, t_up + 150), 0);
    CHECK_EQ(n.state, FLOOR_IDLE);
    // next PTT gets a new stream id
    floor_node_step(&n, FLOOR_EV_PTT_DOWN, t_up + 200);
    CHECK_EQ(n.stream_id, 101);
}

static void test_request_timeout_and_retry(void)
{
    floor_node_t n = node(); uint32_t t = 0, a;
    floor_node_step(&n, FLOOR_EV_PTT_DOWN, t);
    CHECK_EQ(floor_node_step(&n, FLOOR_EV_TICK, t + 99), 0);
    CHECK_EQ(floor_node_step(&n, FLOOR_EV_TICK, t + 100), FLOOR_ACT_SEND_REQUEST);   // retry 2
    CHECK_EQ(floor_node_step(&n, FLOOR_EV_TICK, t + 200), FLOOR_ACT_SEND_REQUEST);   // retry 3
    a = floor_node_step(&n, FLOOR_EV_TICK, t + 300);
    CHECK_EQ(a, FLOOR_ACT_INDICATE_FAIL); CHECK_EQ(n.state, FLOOR_IDLE);              // give up, never STOP_TX (never started)
}

static void test_deny_and_revoke(void)
{
    floor_node_t n = node(); uint32_t a;
    floor_node_step(&n, FLOOR_EV_PTT_DOWN, 0);
    a = floor_node_step(&n, FLOOR_EV_DENY, 10);
    CHECK_EQ(a, FLOOR_ACT_INDICATE_FAIL); CHECK_EQ(n.state, FLOOR_IDLE);
    // revoke while talking must stop TX
    floor_node_step(&n, FLOOR_EV_PTT_DOWN, 100); floor_node_step(&n, FLOOR_EV_GRANT, 110);
    a = floor_node_step(&n, FLOOR_EV_DENY, 500);
    CHECK_EQ(a, FLOOR_ACT_STOP_TX | FLOOR_ACT_INDICATE_FAIL); CHECK_EQ(n.state, FLOOR_IDLE);
}

static void test_link_loss_never_leaves_tx_running(void)
{
    floor_node_t n = node();
    CHECK_EQ(floor_node_step(&n, FLOOR_EV_LINK_LOSS, 0), 0);            // idle: nothing to do
    floor_node_step(&n, FLOOR_EV_PTT_DOWN, 0); floor_node_step(&n, FLOOR_EV_GRANT, 10);
    uint32_t a = floor_node_step(&n, FLOOR_EV_LINK_LOSS, 2000);
    CHECK(a & FLOOR_ACT_STOP_TX); CHECK_EQ(n.state, FLOOR_IDLE);
}

static void test_ptt_up_before_grant_sends_end(void)
{
    floor_node_t n = node();
    floor_node_step(&n, FLOOR_EV_PTT_DOWN, 0);
    uint32_t a = floor_node_step(&n, FLOOR_EV_PTT_UP, 20);
    CHECK_EQ(a, FLOOR_ACT_SEND_END); CHECK_EQ(n.state, FLOOR_ENDING);
    // a late GRANT in ENDING is ignored
    CHECK_EQ(floor_node_step(&n, FLOOR_EV_GRANT, 30), 0); CHECK_EQ(n.state, FLOOR_ENDING);
}

static void test_max_talk(void)
{
    floor_node_t n = node();
    floor_node_step(&n, FLOOR_EV_PTT_DOWN, 0); floor_node_step(&n, FLOOR_EV_GRANT, 10);
    uint32_t t = 10;
    for (; t < 10 + 120000; t += 20) { floor_node_step(&n, FLOOR_EV_VOICE_SENT, t); floor_node_step(&n, FLOOR_EV_TICK, t + 1); }
    uint32_t a = floor_node_step(&n, FLOOR_EV_TICK, t + 1);
    CHECK(a & FLOOR_ACT_STOP_TX); CHECK(a & FLOOR_ACT_SEND_END); CHECK(a & FLOOR_ACT_WARN_MAX_TALK);
    CHECK_EQ(n.state, FLOOR_ENDING);
}

static void test_repress_during_ending(void)
{
    floor_node_t n = node();
    floor_node_step(&n, FLOOR_EV_PTT_DOWN, 0); floor_node_step(&n, FLOOR_EV_GRANT, 10);
    floor_node_step(&n, FLOOR_EV_PTT_UP, 500);
    uint32_t a = floor_node_step(&n, FLOOR_EV_PTT_DOWN, 520);
    CHECK_EQ(a, FLOOR_ACT_SEND_REQUEST); CHECK_EQ(n.state, FLOOR_REQUESTING); CHECK_EQ(n.stream_id, 101);
}

static void test_time_wrap(void)
{
    floor_node_t n = node(); uint32_t t = 0xFFFFFFF0u;
    floor_node_step(&n, FLOOR_EV_PTT_DOWN, t);
    CHECK_EQ(floor_node_step(&n, FLOOR_EV_TICK, t + 50), 0);                       // wraps past 0
    CHECK_EQ(floor_node_step(&n, FLOOR_EV_TICK, t + 100), FLOOR_ACT_SEND_REQUEST);
}

static void test_coordinator(void)
{
    floor_coord_t c; floor_coord_init(&c, NULL);
    CHECK_EQ(floor_coord_request(&c, 1, 100, 0), FLOOR_COORD_GRANT);
    CHECK_EQ(floor_coord_request(&c, 2, 200, 5), FLOOR_COORD_DENY);        // busy
    CHECK_EQ(floor_coord_request(&c, 1, 100, 6), FLOOR_COORD_GRANT);       // duplicate request idempotent
    floor_coord_renew(&c, 2, 200, 700);                                     // non-holder cannot renew
    CHECK(!floor_coord_tick(&c, 749)); CHECK(c.held);
    floor_coord_renew(&c, 1, 100, 700);                                     // holder renews -> expires 1450
    CHECK(!floor_coord_tick(&c, 1000)); CHECK(c.held);
    CHECK(floor_coord_tick(&c, 1450)); CHECK(!c.held);                      // lease expiry releases
    CHECK(!floor_coord_tick(&c, 1451));                                     // reported once
    // END releases; duplicates are harmless
    CHECK_EQ(floor_coord_request(&c, 3, 300, 2000), FLOOR_COORD_GRANT);
    floor_coord_end(&c, 4, 300); CHECK(c.held);                             // wrong sender ignored
    floor_coord_end(&c, 3, 300); CHECK(!c.held);
    floor_coord_end(&c, 3, 300); CHECK(!c.held);
    // same node, new stream (lost END): hand over instead of deny
    CHECK_EQ(floor_coord_request(&c, 5, 500, 3000), FLOOR_COORD_GRANT);
    CHECK_EQ(floor_coord_request(&c, 5, 501, 3100), FLOOR_COORD_GRANT); CHECK_EQ(c.stream_id, 501);
    // expired lease: a stale request from the old holder arriving late is re-evaluated
    CHECK_EQ(floor_coord_request(&c, 6, 600, 5000), FLOOR_COORD_GRANT);
}

static void test_pick_deterministic(void)
{
    floor_request_t r[] = { {100, 9, 1}, {100, 3, 2}, {101, 1, 3}, {100, 5, 4} };
    CHECK_EQ(floor_coord_pick(r, 4), 1);    // earliest time, then lowest sender id
    floor_request_t w[] = { {0xFFFFFFF0u, 2, 1}, {5, 1, 2} };
    CHECK_EQ(floor_coord_pick(w, 2), 0);    // wrap-safe: 0xFFFFFFF0 is earlier than 5
    CHECK_EQ(floor_coord_pick(r, 1), 0);
}

int main(void)
{
    test_happy_path(); test_request_timeout_and_retry(); test_deny_and_revoke();
    test_link_loss_never_leaves_tx_running(); test_ptt_up_before_grant_sends_end();
    test_max_talk(); test_repress_during_ending(); test_time_wrap();
    test_coordinator(); test_pick_deterministic();
    printf("test_floor: %d checks, %d failures\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
