#include "rwp.h"
#include <string.h>

static void put_u16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
static void put_u32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v;
}
static uint16_t get_u16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }
static uint32_t get_u32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static int validate_fields(const rwp_header_t *h)
{
    if (h->type > RWP_TYPE_MAX_) return RWP_ERR_FIELD;
    if (h->codec > RWP_CODEC_MAX_) return RWP_ERR_FIELD;
    if (h->target_type > RWP_TARGET_MAX_) return RWP_ERR_FIELD;
    if (h->target_type == RWP_TARGET_GROUP && h->target_id != 0) return RWP_ERR_FIELD;
    return RWP_OK;
}

int rwp_encode(uint8_t *out, size_t cap, const rwp_header_t *h,
               const void *payload, size_t payload_len)
{
    if (!out || !h) return RWP_ERR_ARG;
    if (payload_len > RWP_MAX_PAYLOAD) return RWP_ERR_PAYLOAD_LEN;
    if (payload_len > 0 && !payload) return RWP_ERR_ARG;
    if (cap < RWP_HEADER_LEN + payload_len) return RWP_ERR_ARG;
    int v = validate_fields(h);
    if (v != RWP_OK) return v;

    uint8_t *p = out;
    put_u16(p, RWP_MAGIC);            p += 2;
    *p++ = RWP_VERSION;
    *p++ = h->type;
    *p++ = h->codec;
    put_u16(p, h->flags);             p += 2;
    put_u16(p, RWP_HEADER_LEN);       p += 2;
    put_u32(p, h->group_id);          p += 4;
    put_u32(p, h->sender_id);         p += 4;
    *p++ = h->target_type;
    put_u32(p, h->target_id);         p += 4;
    put_u32(p, h->stream_id);         p += 4;
    put_u32(p, h->sequence);          p += 4;
    put_u32(p, h->capture_time);      p += 4;
    put_u16(p, (uint16_t)payload_len); p += 2;
    if (payload_len) memcpy(p, payload, payload_len);
    return (int)(RWP_HEADER_LEN + payload_len);
}

int rwp_decode(const uint8_t *in, size_t len, rwp_header_t *h, const uint8_t **payload)
{
    if (!in || !h || !payload) return RWP_ERR_ARG;
    *payload = NULL;
    if (len < RWP_HEADER_LEN) return RWP_ERR_SHORT;
    if (get_u16(in) != RWP_MAGIC) return RWP_ERR_MAGIC;
    if (in[2] != RWP_VERSION) return RWP_ERR_VERSION;

    rwp_header_t t;
    memset(&t, 0, sizeof t);
    t.version      = in[2];
    t.type         = in[3];
    t.codec        = in[4];
    t.flags        = get_u16(in + 5);
    t.header_len   = get_u16(in + 7);
    t.group_id     = get_u32(in + 9);
    t.sender_id    = get_u32(in + 13);
    t.target_type  = in[17];
    t.target_id    = get_u32(in + 18);
    t.stream_id    = get_u32(in + 22);
    t.sequence     = get_u32(in + 26);
    t.capture_time = get_u32(in + 30);
    t.payload_len  = get_u16(in + 34);

    if (t.header_len < RWP_HEADER_LEN || t.header_len > len) return RWP_ERR_HEADER_LEN;
    if (t.payload_len > RWP_MAX_PAYLOAD) return RWP_ERR_PAYLOAD_LEN;
    if ((size_t)t.header_len + t.payload_len > len) return RWP_ERR_PAYLOAD_LEN;
    int v = validate_fields(&t);
    if (v != RWP_OK) return v;

    *h = t;
    *payload = in + t.header_len;
    return RWP_OK;
}

bool rwp_seq_after(uint32_t a, uint32_t b)
{
    return a != b && (uint32_t)(a - b) < 0x80000000u;
}

int rwp_control_encode(uint8_t *out, size_t cap, const rwp_control_t *c)
{
    if (!out || !c) return RWP_ERR_ARG;
    if (cap < RWP_CONTROL_LEN) return RWP_ERR_ARG;
    if (c->ctrl == 0 || c->ctrl > RWP_CTRL_MAX_) return RWP_ERR_FIELD;
    out[0] = c->ctrl;
    put_u32(out + 1, c->stream_id);
    put_u32(out + 5, c->lease_ms);
    return (int)RWP_CONTROL_LEN;
}

int rwp_control_decode(const uint8_t *in, size_t len, rwp_control_t *c)
{
    if (!in || !c) return RWP_ERR_ARG;
    if (len < RWP_CONTROL_LEN) return RWP_ERR_SHORT;
    if (in[0] == 0 || in[0] > RWP_CTRL_MAX_) return RWP_ERR_FIELD;
    c->ctrl = in[0];
    c->stream_id = get_u32(in + 1);
    c->lease_ms = get_u32(in + 5);
    return RWP_OK;
}
