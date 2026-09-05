#include "floor.h"

floor_cfg_t floor_cfg_default(void)
{
    floor_cfg_t c = {
        .grant_timeout_ms  = 100,
        .request_retries   = 3,
        .lease_ms          = 750,
        .renew_interval_ms = 250,
        .max_talk_ms       = 120000,
        .end_repeat        = 3,
        .end_interval_ms   = 50,
    };
    return c;
}

static bool elapsed(uint32_t now, uint32_t since, uint32_t ms)
{
    return (uint32_t)(now - since) >= ms;
}

static void enter(floor_node_t *n, floor_state_t s, uint32_t now)
{
    n->state = s;
    n->t_entered = now;
}

void floor_node_init(floor_node_t *n, const floor_cfg_t *cfg, uint32_t first_stream_id)
{
    *n = (floor_node_t){0};
    n->cfg = cfg ? *cfg : floor_cfg_default();
    n->state = FLOOR_IDLE;
    n->next_stream_id = first_stream_id ? first_stream_id : 1;
}

static uint32_t begin_ending(floor_node_t *n, uint32_t now)
{
    enter(n, FLOOR_ENDING, now);
    n->ends_sent = 1;
    n->t_last_end = now;
    return FLOOR_ACT_STOP_TX | FLOOR_ACT_SEND_END;
}

static uint32_t fail_to_idle(floor_node_t *n, uint32_t now)
{
    enter(n, FLOOR_IDLE, now);
    return FLOOR_ACT_STOP_TX | FLOOR_ACT_INDICATE_FAIL;
}

uint32_t floor_node_step(floor_node_t *n, floor_event_t ev, uint32_t now)
{
    uint32_t act = 0;

    if (ev == FLOOR_EV_LINK_LOSS) {
        if (n->state == FLOOR_IDLE) return 0;
        return fail_to_idle(n, now);   // never keep TX running without a link
    }

    switch (n->state) {
    case FLOOR_IDLE:
        if (ev == FLOOR_EV_PTT_DOWN) {
            n->stream_id = n->next_stream_id++;
            if (n->next_stream_id == 0) n->next_stream_id = 1;
            n->requests_sent = 1;
            enter(n, FLOOR_REQUESTING, now);
            act |= FLOOR_ACT_SEND_REQUEST;
        }
        break;

    case FLOOR_REQUESTING:
        if (ev == FLOOR_EV_PTT_UP) {
            // Released before grant: send END so a late GRANT is cleaned up.
            act |= begin_ending(n, now) & ~FLOOR_ACT_STOP_TX;
        } else if (ev == FLOOR_EV_GRANT) {
            enter(n, FLOOR_TALKING, now);
            n->t_talk_start = now;
            n->t_last_renew = now;
            act |= FLOOR_ACT_START_TX | FLOOR_ACT_INDICATE_GRANT;
        } else if (ev == FLOOR_EV_DENY) {
            act |= fail_to_idle(n, now) & ~FLOOR_ACT_STOP_TX;
        } else if (ev == FLOOR_EV_TICK && elapsed(now, n->t_entered, n->cfg.grant_timeout_ms)) {
            if (n->requests_sent < n->cfg.request_retries) {
                n->requests_sent++;
                n->t_entered = now;
                act |= FLOOR_ACT_SEND_REQUEST;
            } else {
                act |= fail_to_idle(n, now) & ~FLOOR_ACT_STOP_TX;
            }
        }
        break;

    case FLOOR_TALKING:
        if (ev == FLOOR_EV_PTT_UP) {
            act |= begin_ending(n, now);
        } else if (ev == FLOOR_EV_DENY) {
            act |= fail_to_idle(n, now);          // coordinator revoked
        } else if (ev == FLOOR_EV_VOICE_SENT) {
            n->t_last_renew = now;
        } else if (ev == FLOOR_EV_TICK) {
            if (elapsed(now, n->t_talk_start, n->cfg.max_talk_ms)) {
                act |= begin_ending(n, now) | FLOOR_ACT_WARN_MAX_TALK;
            } else if (elapsed(now, n->t_last_renew, n->cfg.renew_interval_ms)) {
                n->t_last_renew = now;
                act |= FLOOR_ACT_SEND_RENEW;
            }
        }
        break;

    case FLOOR_ENDING:
        if (ev == FLOOR_EV_PTT_DOWN) {
            // Re-press during END burst: finish this stream, start a new one.
            n->stream_id = n->next_stream_id++;
            if (n->next_stream_id == 0) n->next_stream_id = 1;
            n->requests_sent = 1;
            enter(n, FLOOR_REQUESTING, now);
            act |= FLOOR_ACT_SEND_REQUEST;
        } else if (ev == FLOOR_EV_TICK && elapsed(now, n->t_last_end, n->cfg.end_interval_ms)) {
            if (n->ends_sent < n->cfg.end_repeat) {
                n->ends_sent++;
                n->t_last_end = now;
                act |= FLOOR_ACT_SEND_END;
            } else {
                enter(n, FLOOR_IDLE, now);
            }
        }
        break;
    }
    return act;
}

// ---------------- Coordinator ----------------

void floor_coord_init(floor_coord_t *c, const floor_cfg_t *cfg)
{
    *c = (floor_coord_t){0};
    c->cfg = cfg ? *cfg : floor_cfg_default();
}

floor_coord_result_t floor_coord_request(floor_coord_t *c, uint32_t sender_id,
                                         uint32_t stream_id, uint32_t now)
{
    floor_coord_tick(c, now);
    if (c->held) {
        if (c->holder_id == sender_id && c->stream_id == stream_id) {
            c->t_expires = now + c->cfg.lease_ms;    // duplicate REQUEST: re-grant
            return FLOOR_COORD_GRANT;
        }
        if (c->holder_id == sender_id) {
            // Same node, new stream: it released (END may have been lost). Hand over.
            c->stream_id = stream_id;
            c->t_granted = now;
            c->t_expires = now + c->cfg.lease_ms;
            return FLOOR_COORD_GRANT;
        }
        return FLOOR_COORD_DENY;
    }
    c->held = true;
    c->holder_id = sender_id;
    c->stream_id = stream_id;
    c->t_granted = now;
    c->t_expires = now + c->cfg.lease_ms;
    return FLOOR_COORD_GRANT;
}

void floor_coord_renew(floor_coord_t *c, uint32_t sender_id, uint32_t stream_id, uint32_t now)
{
    if (c->held && c->holder_id == sender_id && c->stream_id == stream_id) {
        c->t_expires = now + c->cfg.lease_ms;
    }
}

void floor_coord_end(floor_coord_t *c, uint32_t sender_id, uint32_t stream_id)
{
    if (c->held && c->holder_id == sender_id && c->stream_id == stream_id) {
        c->held = false;
    }
}

bool floor_coord_tick(floor_coord_t *c, uint32_t now)
{
    if (c->held && (int32_t)(now - c->t_expires) >= 0) {
        c->held = false;
        return true;
    }
    return false;
}

size_t floor_coord_pick(const floor_request_t *reqs, size_t n)
{
    size_t best = 0;
    for (size_t i = 1; i < n; i++) {
        int32_t dt = (int32_t)(reqs[i].recv_time_ms - reqs[best].recv_time_ms);
        if (dt < 0 || (dt == 0 && reqs[i].sender_id < reqs[best].sender_id)) best = i;
    }
    return best;
}
