#include "jitter.h"
#include <string.h>

jb_cfg_t jb_cfg_default(void)
{
    jb_cfg_t c = { .frame_ms = 20, .target_ms = 40, .max_ms = 80, .min_ms = 20,
                   .late_grow_count = 3, .shrink_after_ms = 2000 };
    return c;
}

static bool seq_after(uint32_t a, uint32_t b) { return a != b && (uint32_t)(a - b) < 0x80000000u; }
static jb_slot_t *slot_for(jb_t *jb, uint32_t seq) { return &jb->slots[seq % JB_MAX_FRAMES]; }

void jb_init(jb_t *jb, const jb_cfg_t *cfg)
{
    memset(jb, 0, sizeof *jb);
    jb->cfg = cfg ? *cfg : jb_cfg_default();
    if (jb->cfg.frame_ms == 0) jb->cfg.frame_ms = 20;
    jb->depth_frames = (uint16_t)(jb->cfg.target_ms / jb->cfg.frame_ms);
    if (jb->depth_frames == 0) jb->depth_frames = 1;
    jb->st.depth_ms = (uint16_t)(jb->depth_frames * jb->cfg.frame_ms);
}

void jb_reset(jb_t *jb, uint32_t stream_id)
{
    for (int i = 0; i < JB_MAX_FRAMES; i++) jb->slots[i].used = false;
    jb->started = false; jb->have_head = false; jb->stream_id = stream_id;
    jb->late_window = 0; jb->t_over_since = 0;
}

uint16_t jb_buffered(const jb_t *jb)
{
    if (!jb->have_head) return 0;
    uint16_t n = 0;
    for (uint32_t s = jb->head_seq; s != jb->head_seq + JB_MAX_FRAMES; s++) {
        const jb_slot_t *sl = &jb->slots[s % JB_MAX_FRAMES];
        if (sl->used && sl->seq == s) n++;
    }
    return n;
}

const jb_stats_t *jb_stats(const jb_t *jb) { return &jb->st; }

static void adapt_grow(jb_t *jb)
{
    uint16_t max_frames = (uint16_t)(jb->cfg.max_ms / jb->cfg.frame_ms);
    if (jb->depth_frames < max_frames && jb->depth_frames < JB_MAX_FRAMES - 1) {
        jb->depth_frames++; jb->st.grow++; jb->st.depth_ms = (uint16_t)(jb->depth_frames * jb->cfg.frame_ms);
    }
}

static void adapt_shrink(jb_t *jb)
{
    uint16_t min_frames = (uint16_t)(jb->cfg.min_ms / jb->cfg.frame_ms);
    if (min_frames == 0) min_frames = 1;
    if (jb->depth_frames > min_frames) {
        jb->depth_frames--; jb->st.shrink++; jb->st.depth_ms = (uint16_t)(jb->depth_frames * jb->cfg.frame_ms);
    }
}

jb_put_result_t jb_put(jb_t *jb, uint32_t stream_id, uint32_t seq, const uint8_t *payload, size_t len, uint32_t now)
{
    if (!payload || len == 0 || len > JB_MAX_PAYLOAD) return JB_PUT_ERR;
    if (!jb->have_head || jb->stream_id != stream_id) {
        jb_reset(jb, stream_id);
        jb->have_head = true; jb->head_seq = seq;
    }
    // late: older than head
    if (seq_after(jb->head_seq, seq)) {
        jb->st.late++;
        if ((uint32_t)(now - jb->t_window) > 1000) { jb->t_window = now; jb->late_window = 0; }
        if (++jb->late_window >= jb->cfg.late_grow_count) { jb->late_window = 0; adapt_grow(jb); }
        return JB_PUT_LATE;
    }
    // too far ahead: everything in between is lost; restart from here
    if ((uint32_t)(seq - jb->head_seq) >= JB_MAX_FRAMES) {
        jb->st.too_far++;
        jb_reset(jb, stream_id);
        jb->have_head = true; jb->head_seq = seq;
    }
    jb_slot_t *sl = slot_for(jb, seq);
    if (sl->used && sl->seq == seq) { jb->st.duplicate++; return JB_PUT_DUPLICATE; }
    sl->used = true; sl->seq = seq; sl->len = (uint16_t)len;
    memcpy(sl->payload, payload, len);
    jb->st.put_ok++;
    return JB_PUT_OK;
}

jb_get_result_t jb_get(jb_t *jb, uint8_t *payload, size_t cap, size_t *len, uint32_t now)
{
    *len = 0;
    if (!jb->have_head) return JB_GET_WAIT;
    uint16_t buffered = jb_buffered(jb);

    if (!jb->started) {
        if (buffered < jb->depth_frames) return JB_GET_WAIT;
        jb->started = true;
    }

    // adaptive shrink: sustained over-depth means we are adding latency for nothing
    if (buffered > jb->depth_frames + 1) {
        if (jb->t_over_since == 0) jb->t_over_since = now ? now : 1;
        else if ((uint32_t)(now - jb->t_over_since) >= jb->cfg.shrink_after_ms) {
            adapt_shrink(jb);
            jb->t_over_since = 0;
            // drop one frame to realise the shorter depth
            jb_slot_t *drop = slot_for(jb, jb->head_seq);
            if (drop->used && drop->seq == jb->head_seq) drop->used = false;
            jb->head_seq++;
        }
    } else {
        jb->t_over_since = 0;
    }

    jb_slot_t *sl = slot_for(jb, jb->head_seq);
    if (sl->used && sl->seq == jb->head_seq) {
        if (cap < sl->len) return JB_GET_UNDERRUN;   // caller bug; treat as dropout
        memcpy(payload, sl->payload, sl->len); *len = sl->len;
        sl->used = false; jb->head_seq++; jb->st.frames_played++;
        return JB_GET_FRAME;
    }
    // head missing: is anything newer buffered? then it is a gap; else we ran dry
    if (buffered > 0) { jb->head_seq++; jb->st.gap++; return JB_GET_GAP; }
    jb->st.underrun++;
    jb->started = false;          // re-prefill before resuming
    return JB_GET_UNDERRUN;
}
