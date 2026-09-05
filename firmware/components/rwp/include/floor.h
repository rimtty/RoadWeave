// Floor control (docs/voice-networking.md §6): node-side PTT state machine and
// coordinator-side floor table. Pure C11, time injected as monotonic ms.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t grant_timeout_ms;   // wait for GRANT before retrying REQUEST (100)
    uint8_t  request_retries;    // REQUEST attempts before giving up (3)
    uint32_t lease_ms;           // coordinator lease length (750)
    uint32_t renew_interval_ms;  // node sends RENEW if no voice packet within this (250)
    uint32_t max_talk_ms;        // continuous PTT limit (120000)
    uint8_t  end_repeat;         // PTT_END repetitions (3)
    uint32_t end_interval_ms;    // spacing between PTT_END repeats (50)
} floor_cfg_t;

// Defaults from the design doc.
floor_cfg_t floor_cfg_default(void);

// ---------------- Node side ----------------
typedef enum {
    FLOOR_IDLE = 0,
    FLOOR_REQUESTING,
    FLOOR_TALKING,
    FLOOR_ENDING,
} floor_state_t;

typedef enum {
    FLOOR_EV_TICK = 0,      // periodic call (>= every ~10 ms) with current time
    FLOOR_EV_PTT_DOWN,
    FLOOR_EV_PTT_UP,
    FLOOR_EV_GRANT,         // GRANT received for our stream_id
    FLOOR_EV_DENY,          // DENY received for our stream_id
    FLOOR_EV_VOICE_SENT,    // application sent a voice packet (counts as renew)
    FLOOR_EV_LINK_LOSS,     // transport reports link down
} floor_event_t;

// Actions the caller must perform after floor_node_step(). Bitmask.
enum {
    FLOOR_ACT_SEND_REQUEST   = 1u << 0,
    FLOOR_ACT_START_TX       = 1u << 1,   // begin capture/encode/send
    FLOOR_ACT_STOP_TX        = 1u << 2,   // stop capture immediately
    FLOOR_ACT_SEND_RENEW     = 1u << 3,
    FLOOR_ACT_SEND_END       = 1u << 4,   // send PTT_END (may repeat)
    FLOOR_ACT_INDICATE_GRANT = 1u << 5,   // beep / UI
    FLOOR_ACT_INDICATE_FAIL  = 1u << 6,   // denied, timed out, link loss
    FLOOR_ACT_WARN_MAX_TALK  = 1u << 7,
};

typedef struct {
    floor_cfg_t cfg;
    floor_state_t state;
    uint32_t stream_id;      // current PTT session
    uint32_t next_stream_id; // allocated per PTT_DOWN
    uint32_t t_entered;      // time state was entered
    uint32_t t_last_renew;   // last voice/renew sent
    uint32_t t_talk_start;
    uint32_t t_last_end;
    uint8_t  requests_sent;
    uint8_t  ends_sent;
} floor_node_t;

void     floor_node_init(floor_node_t *n, const floor_cfg_t *cfg, uint32_t first_stream_id);
// Feed an event at time now_ms. Returns action bitmask.
uint32_t floor_node_step(floor_node_t *n, floor_event_t ev, uint32_t now_ms);

// ---------------- Coordinator side ----------------
typedef struct {
    floor_cfg_t cfg;
    bool     held;
    uint32_t holder_id;
    uint32_t stream_id;
    uint32_t t_granted;
    uint32_t t_expires;
} floor_coord_t;

typedef enum { FLOOR_COORD_GRANT = 0, FLOOR_COORD_DENY = 1 } floor_coord_result_t;

void floor_coord_init(floor_coord_t *c, const floor_cfg_t *cfg);
// A REQUEST arrived from sender for stream at now. Idempotent for the current holder.
floor_coord_result_t floor_coord_request(floor_coord_t *c, uint32_t sender_id,
                                         uint32_t stream_id, uint32_t now_ms);
// A voice or RENEW packet arrived from sender/stream. Extends lease if it is the holder.
void floor_coord_renew(floor_coord_t *c, uint32_t sender_id, uint32_t stream_id, uint32_t now_ms);
// PTT_END arrived. Releases if it matches the holder. Idempotent.
void floor_coord_end(floor_coord_t *c, uint32_t sender_id, uint32_t stream_id);
// Call periodically. Returns true if the lease expired and the floor was released.
bool floor_coord_tick(floor_coord_t *c, uint32_t now_ms);

// Deterministic arbitration for requests received in the same tick:
// earliest recv_time wins, ties broken by lowest sender_id. Returns index into reqs.
typedef struct { uint32_t recv_time_ms; uint32_t sender_id; uint32_t stream_id; } floor_request_t;
size_t floor_coord_pick(const floor_request_t *reqs, size_t n);

#ifdef __cplusplus
}
#endif
