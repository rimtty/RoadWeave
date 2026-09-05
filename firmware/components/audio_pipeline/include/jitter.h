// Jitter buffer for RoadWeave voice (docs/voice-networking.md §7).
// Fixed frame duration, sequence-ordered slots, adaptive target depth.
// Pure C11; time injected as monotonic ms. No allocation after init.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define JB_MAX_FRAMES   16
#define JB_MAX_PAYLOAD  200   // ADPCM 20 ms = 164 B, Opus <= ~100 B

typedef struct {
    uint16_t frame_ms;        // 20
    uint16_t target_ms;       // initial depth before playout starts (20..40)
    uint16_t max_ms;          // adaptive upper bound (80)
    uint16_t min_ms;          // adaptive lower bound (20)
    uint16_t late_grow_count; // late frames within a window that trigger +1 frame of depth (3)
    uint16_t shrink_after_ms; // sustained over-depth before shrinking one frame (2000)
    uint16_t shrink_holdoff_ms; // no shrink within this time after an underrun/gap/grow (10000)
} jb_cfg_t;

jb_cfg_t jb_cfg_default(void);

typedef enum {
    JB_PUT_OK = 0,
    JB_PUT_LATE,        // older than the playout head: dropped, counted
    JB_PUT_DUPLICATE,
    JB_PUT_TOO_FAR,     // beyond capacity: buffer reset to this frame (burst loss / long stall)
    JB_PUT_ERR,         // bad args / payload too large
} jb_put_result_t;

typedef enum {
    JB_GET_FRAME = 0,   // payload filled
    JB_GET_GAP,         // missing frame at head: caller plays PLC/silence
    JB_GET_WAIT,        // prefilling (or empty): caller plays silence, not counted as underrun
    JB_GET_UNDERRUN,    // buffer ran dry after playout started
} jb_get_result_t;

typedef struct {
    uint32_t put_ok, late, duplicate, too_far, gap, underrun, frames_played, grow, shrink;
    uint16_t depth_ms;   // current target depth
} jb_stats_t;

typedef struct {
    uint16_t len;
    uint32_t seq;
    uint32_t tag;     // caller data carried with the frame (e.g. capture_time for latency measurement)
    bool     used;
    uint8_t  payload[JB_MAX_PAYLOAD];
} jb_slot_t;

typedef struct {
    jb_cfg_t  cfg;
    jb_slot_t slots[JB_MAX_FRAMES];
    bool      started;      // playout started (prefill satisfied)
    bool      have_head;
    uint32_t  head_seq;     // next sequence to play
    uint32_t  stream_id;
    uint16_t  depth_frames; // adaptive target in frames
    uint16_t  late_window;  // late count in current window
    uint32_t  t_window;     // window start
    uint32_t  t_over_since; // time buffered depth first exceeded target+1 (0 = not)
    uint32_t  t_last_trouble; // last underrun/gap/grow time (shrink hold-off reference)
    jb_stats_t st;
} jb_t;

void jb_init(jb_t *jb, const jb_cfg_t *cfg);
// Reset for a new stream (flush everything, keep adaptive depth).
void jb_reset(jb_t *jb, uint32_t stream_id);
jb_put_result_t jb_put(jb_t *jb, uint32_t stream_id, uint32_t seq, const uint8_t *payload, size_t len, uint32_t now_ms);
// Same, carrying a caller tag that jb_get_tag() returns with the frame.
jb_put_result_t jb_put_tag(jb_t *jb, uint32_t stream_id, uint32_t seq, const uint8_t *payload, size_t len, uint32_t tag, uint32_t now_ms);
// Called once per frame period by the playout clock.
jb_get_result_t jb_get(jb_t *jb, uint8_t *payload, size_t cap, size_t *len, uint32_t now_ms);
// Same, also returning the tag stored with the frame (valid only for JB_GET_FRAME).
jb_get_result_t jb_get_tag(jb_t *jb, uint8_t *payload, size_t cap, size_t *len, uint32_t *tag, uint32_t now_ms);
// Frames currently buffered ahead of head.
uint16_t jb_buffered(const jb_t *jb);
const jb_stats_t *jb_stats(const jb_t *jb);

#ifdef __cplusplus
}
#endif
