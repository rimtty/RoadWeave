// RoadWeave host simulator: 1 coordinator + N nodes over a lossy/jittery channel.
// Links the real components (rwp, floor, jitter, adpcm) and checks invariants
// that are hard to hit on a bench: floor contention, lease expiry, lost PTT_END,
// burst loss / burst delay vs jitter buffer. Exit code != 0 on any invariant violation.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "rwp.h"
#include "floor.h"
#include "jitter.h"
#include "adpcm.h"

#define MAX_NODES 4
#define FRAME_MS 20
#define FRAME_SAMPLES 320
#define Q_CAP 4096
static const double PI = 3.14159265358979323846; // M_PI is not part of C11.

static uint32_t rng_state = 1;
static uint32_t rnd(void) { rng_state = rng_state * 1664525u + 1013904223u; return rng_state >> 8; }
static double rnd01(void) { return (rnd() & 0xFFFFFF) / 16777216.0; }

typedef struct { uint32_t deliver_at; uint8_t to; uint8_t data[RWP_HEADER_LEN + 200]; size_t len; bool used; } pkt_t;
typedef struct {
    double loss;            // 0..1 independent loss
    uint32_t base_ms;       // one-way base delay
    uint32_t jitter_ms;     // uniform extra delay 0..jitter
    uint32_t burst_start, burst_len_ms, burst_extra_ms;  // one-off burst delay window
    pkt_t q[Q_CAP];
} channel_t;

static channel_t ch;
static uint32_t chan_send(uint32_t now, uint8_t to, const uint8_t *d, size_t len)
{
    if (rnd01() < ch.loss) return 0;
    uint32_t delay = ch.base_ms + (ch.jitter_ms ? rnd() % (ch.jitter_ms + 1) : 0);
    if (ch.burst_len_ms && now >= ch.burst_start && now < ch.burst_start + ch.burst_len_ms) delay += ch.burst_extra_ms;
    for (int i = 0; i < Q_CAP; i++) if (!ch.q[i].used) {
        ch.q[i].used = true; ch.q[i].to = to; ch.q[i].deliver_at = now + delay; ch.q[i].len = len; memcpy(ch.q[i].data, d, len); return 1;
    }
    fprintf(stderr, "channel queue full\n"); exit(2);
}

// ---------------- coordinator ----------------
#define COORD 0xFF
static floor_coord_t coord;
static uint32_t coord_grants = 0, coord_denies = 0;

// ---------------- nodes ----------------
typedef struct {
    uint8_t id; uint32_t node_id;
    floor_node_t fn; jb_t jb; adpcm_state_t enc;
    bool tx_running; uint32_t seq; uint32_t stream_for_seq;
    uint32_t frames_sent, frames_played, gaps, underruns;
    uint64_t lat_sum; uint32_t lat_n; uint32_t lat_last_avg;   // playout latency (capture -> playout) of played frames
    bool ptt;                      // physical PTT
    uint32_t tx_without_floor;     // invariant counter
} node_t;
static node_t nodes[MAX_NODES]; static int n_nodes = 2;
static uint32_t holder_conflicts = 0;   // >1 node in TALKING at once

static void send_ctrl(uint32_t now, node_t *n, uint8_t ctrl, uint32_t stream, uint32_t lease, uint8_t to)
{
    rwp_control_t c = { .ctrl = ctrl, .stream_id = stream, .lease_ms = lease }; uint8_t body[RWP_CONTROL_LEN];
    rwp_control_encode(body, sizeof body, &c);
    rwp_header_t h = { .version = RWP_VERSION, .type = RWP_TYPE_CONTROL, .group_id = 1, .sender_id = n ? n->node_id : 0xC0,
                       .target_type = RWP_TARGET_NODE, .target_id = to == COORD ? 0xC0 : nodes[to].node_id, .stream_id = stream, .sequence = 0, .capture_time = now };
    uint8_t pkt[RWP_HEADER_LEN + RWP_CONTROL_LEN]; int len = rwp_encode(pkt, sizeof pkt, &h, body, sizeof body);
    chan_send(now, to, pkt, (size_t)len);
}

static void node_apply_actions(uint32_t now, node_t *n, uint32_t act)
{
    if (act & FLOOR_ACT_SEND_REQUEST) send_ctrl(now, n, RWP_CTRL_FLOOR_REQUEST, n->fn.stream_id, 0, COORD);
    if (act & FLOOR_ACT_START_TX) { n->tx_running = true; n->seq = 0; n->stream_for_seq = n->fn.stream_id; adpcm_state_init(&n->enc); }
    if (act & FLOOR_ACT_STOP_TX) n->tx_running = false;
    if (act & FLOOR_ACT_SEND_RENEW) send_ctrl(now, n, RWP_CTRL_FLOOR_RENEW, n->fn.stream_id, 0, COORD);
    if (act & FLOOR_ACT_SEND_END) send_ctrl(now, n, RWP_CTRL_PTT_END, n->fn.stream_id, 0, COORD);
}

static void node_tick(uint32_t now, node_t *n)
{
    node_apply_actions(now, n, floor_node_step(&n->fn, FLOOR_EV_TICK, now));
    if (n->tx_running) {
        if (n->fn.state != FLOOR_TALKING) n->tx_without_floor++;
        // one 20 ms voice frame: synthetic tone so the receiver can decode something
        int16_t pcm[FRAME_SAMPLES]; static double ph = 0;
        for (int i = 0; i < FRAME_SAMPLES; i++) { pcm[i] = (int16_t)(8000 * sin(ph)); ph += 2 * PI * 440 / 16000; }
        uint8_t blk[164]; size_t bl = adpcm_encode_block(&n->enc, pcm, FRAME_SAMPLES, blk, sizeof blk);
        rwp_header_t h = { .version = RWP_VERSION, .type = RWP_TYPE_VOICE, .codec = RWP_CODEC_IMA_ADPCM, .group_id = 1, .sender_id = n->node_id,
                           .target_type = RWP_TARGET_GROUP, .stream_id = n->stream_for_seq, .sequence = n->seq++, .capture_time = now };
        uint8_t pkt[RWP_HEADER_LEN + 164]; int len = rwp_encode(pkt, sizeof pkt, &h, blk, bl);
        for (int j = 0; j < n_nodes; j++) if (j != n->id) chan_send(now, (uint8_t)j, pkt, (size_t)len);
        chan_send(now, COORD, pkt, (size_t)len);    // coordinator sees voice too (lease renew)
        n->frames_sent++;
        node_apply_actions(now, n, floor_node_step(&n->fn, FLOOR_EV_VOICE_SENT, now));
    }
    // playout clock
    uint8_t frame[JB_MAX_PAYLOAD]; size_t fl; int16_t out[FRAME_SAMPLES]; uint32_t tag = 0;
    jb_get_result_t r = jb_get_tag(&n->jb, frame, sizeof frame, &fl, &tag, now);
    if (r == JB_GET_FRAME) { if (adpcm_decode_block(frame, fl, out, FRAME_SAMPLES) == FRAME_SAMPLES) { n->frames_played++; n->lat_sum += now - tag; n->lat_n++; } }
    else if (r == JB_GET_GAP) n->gaps++;
    else if (r == JB_GET_UNDERRUN) n->underruns++;
}

static void node_receive(uint32_t now, node_t *n, const uint8_t *d, size_t len)
{
    rwp_header_t h; const uint8_t *pl;
    if (rwp_decode(d, len, &h, &pl) != RWP_OK) return;
    if (h.type == RWP_TYPE_CONTROL) {
        rwp_control_t c; if (rwp_control_decode(pl, h.payload_len, &c) != RWP_OK) return;
        if (c.stream_id != n->fn.stream_id) return;   // stale
        if (c.ctrl == RWP_CTRL_FLOOR_GRANT) node_apply_actions(now, n, floor_node_step(&n->fn, FLOOR_EV_GRANT, now));
        else if (c.ctrl == RWP_CTRL_FLOOR_DENY) node_apply_actions(now, n, floor_node_step(&n->fn, FLOOR_EV_DENY, now));
    } else if (h.type == RWP_TYPE_VOICE) {
        jb_put_tag(&n->jb, h.stream_id ^ (h.sender_id << 8), h.sequence, pl, h.payload_len, h.capture_time, now);
    }
}

static void coord_receive(uint32_t now, const uint8_t *d, size_t len)
{
    rwp_header_t h; const uint8_t *pl;
    if (rwp_decode(d, len, &h, &pl) != RWP_OK) return;
    int from = -1; for (int j = 0; j < n_nodes; j++) if (nodes[j].node_id == h.sender_id) from = j;
    if (from < 0) return;
    if (h.type == RWP_TYPE_VOICE) { floor_coord_renew(&coord, h.sender_id, h.stream_id, now); return; }
    rwp_control_t c; if (rwp_control_decode(pl, h.payload_len, &c) != RWP_OK) return;
    if (c.ctrl == RWP_CTRL_FLOOR_REQUEST) {
        floor_coord_result_t r = floor_coord_request(&coord, h.sender_id, c.stream_id, now);
        if (r == FLOOR_COORD_GRANT) { coord_grants++; send_ctrl(now, NULL, RWP_CTRL_FLOOR_GRANT, c.stream_id, coord.cfg.lease_ms, (uint8_t)from); }
        else { coord_denies++; send_ctrl(now, NULL, RWP_CTRL_FLOOR_DENY, c.stream_id, 0, (uint8_t)from); }
    } else if (c.ctrl == RWP_CTRL_FLOOR_RENEW) floor_coord_renew(&coord, h.sender_id, c.stream_id, now);
    else if (c.ctrl == RWP_CTRL_PTT_END) floor_coord_end(&coord, h.sender_id, c.stream_id);
}

static void deliver(uint32_t now)
{
    for (int i = 0; i < Q_CAP; i++) if (ch.q[i].used && (int32_t)(now - ch.q[i].deliver_at) >= 0) {
        ch.q[i].used = false;
        if (ch.q[i].to == COORD) coord_receive(now, ch.q[i].data, ch.q[i].len);
        else node_receive(now, &nodes[ch.q[i].to], ch.q[i].data, ch.q[i].len);
    }
}

static void reset_world(int n, double loss, uint32_t base, uint32_t jitter)
{
    memset(&ch, 0, sizeof ch); ch.loss = loss; ch.base_ms = base; ch.jitter_ms = jitter;
    floor_coord_init(&coord, NULL); coord_grants = coord_denies = 0; holder_conflicts = 0;
    n_nodes = n; memset(nodes, 0, sizeof nodes);
    for (int i = 0; i < n; i++) { nodes[i].id = (uint8_t)i; nodes[i].node_id = 0x100u + (uint32_t)i; floor_node_init(&nodes[i].fn, NULL, 1000u * (uint32_t)(i + 1)); jb_init(&nodes[i].jb, NULL); }
}

static void step(uint32_t now)
{
    deliver(now);
    if (now % FRAME_MS == 0) for (int i = 0; i < n_nodes; i++) node_tick(now, &nodes[i]);
    else for (int i = 0; i < n_nodes; i++) node_apply_actions(now, &nodes[i], floor_node_step(&nodes[i].fn, FLOOR_EV_TICK, now));
    floor_coord_tick(&coord, now);
    int talking = 0; for (int i = 0; i < n_nodes; i++) if (nodes[i].fn.state == FLOOR_TALKING) talking++;
    if (talking > 1) holder_conflicts++;
}

static void ptt(uint32_t now, int i, bool down)
{
    nodes[i].ptt = down;
    node_apply_actions(now, &nodes[i], floor_node_step(&nodes[i].fn, down ? FLOOR_EV_PTT_DOWN : FLOOR_EV_PTT_UP, now));
}

static int fails = 0;
#define EXPECT(cond, ...) do { if (!(cond)) { fails++; printf("  FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

static void scenario_contention(void)
{
    printf("[1] simultaneous PTT, 2 nodes, 5%% loss, 30+-20 ms\n");
    reset_world(2, 0.05, 30, 20);
    uint32_t t = 0; ptt(t, 0, true); ptt(t, 1, true);
    for (; t < 3000; t++) step(t);
    int talking = 0; for (int i = 0; i < 2; i++) if (nodes[i].fn.state == FLOOR_TALKING) talking++;
    EXPECT(talking == 1, "exactly one talker expected, got %d", talking);
    EXPECT(holder_conflicts == 0, "two nodes talked at once (%u ticks)", holder_conflicts);
    EXPECT(coord_denies >= 1, "loser should have been denied");
    printf("  grants %u denies %u conflicts %u\n", coord_grants, coord_denies, holder_conflicts);
}

static void scenario_lost_end_and_handover(void)
{
    printf("[2] PTT_END all lost -> lease expiry frees the floor for the other node\n");
    reset_world(2, 0.0, 20, 0);
    uint32_t t = 0; ptt(t, 0, true);
    for (; t < 1000; t++) step(t);
    EXPECT(nodes[0].fn.state == FLOOR_TALKING, "node0 should be talking");
    ch.loss = 1.0; ptt(t, 0, false);                 // release, but everything is lost now
    for (; t < 1400; t++) step(t);
    ch.loss = 0.0; ptt(t, 1, true);
    uint32_t t_req = t; bool granted = false;
    for (; t < t_req + 3000; t++) { step(t); if (nodes[1].fn.state == FLOOR_TALKING) { granted = true; break; } }
    EXPECT(granted, "node1 never got the floor after node0's END was lost");
    printf("  handover after %u ms (lease 750 ms + request retries)\n", t - t_req);
    EXPECT(t - t_req <= 1500, "handover took too long: %u ms", t - t_req);
}

static void scenario_link_loss_stops_tx(void)
{
    printf("[3] link loss while talking -> TX stops immediately, floor released by lease\n");
    reset_world(2, 0.0, 20, 0);
    uint32_t t = 0; ptt(t, 0, true); for (; t < 500; t++) step(t);
    node_apply_actions(t, &nodes[0], floor_node_step(&nodes[0].fn, FLOOR_EV_LINK_LOSS, t));
    EXPECT(!nodes[0].tx_running, "TX still running after link loss");
    for (; t < 2000; t++) step(t);
    EXPECT(!coord.held, "coordinator still holds a lease 1.5 s after the talker vanished");
}

static void scenario_voice_loss(double loss)
{
    printf("[4] voice over %.0f%% loss, 40+-30 ms: gaps should track loss, no conflicts\n", loss * 100);
    reset_world(2, loss, 40, 30);
    uint32_t t = 0; ptt(t, 0, true); for (; t < 10000; t++) step(t); ptt(t, 0, false); for (; t < 11000; t++) step(t);
    node_t *rx = &nodes[1];
    double gap_rate = (double)rx->gaps / (rx->frames_played + rx->gaps + 1);
    printf("  sent %u played %u gaps %u underruns %u depth %u ms (gap rate %.1f%%)\n", nodes[0].frames_sent, rx->frames_played, rx->gaps, rx->underruns, jb_stats(&rx->jb)->depth_ms, gap_rate * 100);
    EXPECT(rx->frames_played > nodes[0].frames_sent * (1 - loss) * 0.85, "too few frames played");
    EXPECT(fabs(gap_rate - loss) < 0.06 + loss * 0.5, "gap rate %.3f far from loss %.3f", gap_rate, loss);
    EXPECT(nodes[0].tx_without_floor == 0, "voice sent without floor");
}

static void scenario_burst_delay(void)
{
    printf("[5] 400 ms burst delay mid-stream -> underrun once, depth grows, then stable\n");
    reset_world(2, 0.0, 30, 10);
    ch.burst_start = 3000; ch.burst_len_ms = 300; ch.burst_extra_ms = 400;
    uint32_t t = 0; ptt(t, 0, true); for (; t < 8000; t++) step(t);
    node_t *rx = &nodes[1];
    const jb_stats_t *js = jb_stats(&rx->jb);
    // latency of the last 2 seconds only (after the burst settled)
    rx->lat_sum = 0; rx->lat_n = 0; for (; t < 10000; t++) step(t);
    uint32_t lat = rx->lat_n ? (uint32_t)(rx->lat_sum / rx->lat_n) : 0;
    printf("  underruns %u gaps %u grow %u shrink %u trimmed %u depth %u ms | settled playout latency %u ms (base 30 + jitter <=10 + depth %u)\n",
           js->underrun, js->gap, js->grow, js->shrink, js->trimmed, js->depth_ms, lat, js->depth_ms);
    EXPECT(js->underrun >= 1 && js->underrun <= 3, "expected 1-3 underruns, got %u", js->underrun);
    EXPECT(js->grow >= 1, "depth did not grow after the burst");
    EXPECT(js->depth_ms >= 60, "depth should stay >= 60 ms after trouble, is %u", js->depth_ms);
    EXPECT(lat <= 30u + 10u + js->depth_ms + 20u, "playout latency %u ms did not settle to depth after the burst", lat);
}

static void scenario_long_random(void)
{
    printf("[6] 4 nodes, 5 minutes, random PTT presses, 3%% loss, 30+-40 ms: invariants only\n");
    reset_world(4, 0.03, 30, 40);
    uint32_t t = 0; uint32_t next_ev = 200;
    for (; t < 300000; t++) {
        if (t == next_ev) { int i = (int)(rnd() % 4); ptt(t, i, !nodes[i].ptt); next_ev = t + 100 + rnd() % 3000; }
        step(t);
    }
    uint32_t sent = 0, played = 0, tw = 0; for (int i = 0; i < 4; i++) { sent += nodes[i].frames_sent; played += nodes[i].frames_played; tw += nodes[i].tx_without_floor; }
    printf("  grants %u denies %u frames sent %u played(all rx) %u conflicts %u tx_without_floor %u\n", coord_grants, coord_denies, sent, played, holder_conflicts, tw);
    EXPECT(holder_conflicts == 0, "two talkers at once");
    EXPECT(tw == 0, "TX without floor");
    EXPECT(coord_grants > 20, "too few grants for a 5 minute random run");
}

int main(int argc, char **argv)
{
    rng_state = argc > 1 ? (uint32_t)atoi(argv[1]) : 12345;
    scenario_contention(); scenario_lost_end_and_handover(); scenario_link_loss_stops_tx();
    scenario_voice_loss(0.05); scenario_voice_loss(0.20); scenario_burst_delay(); scenario_long_random();
    printf(fails ? "SIM: %d FAILURE(S)\n" : "SIM: all scenarios passed\n", fails);
    return fails ? 1 : 0;
}
