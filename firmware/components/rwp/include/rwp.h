// RWP/0.1 wire format (docs/voice-networking.md §4)
// Pure C11, no ESP-IDF dependency. Network byte order, explicit serialize.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RWP_MAGIC       0x5257u   // "RW"
#define RWP_VERSION     1u
#define RWP_HEADER_LEN  36u       // fixed header size for version 1
#define RWP_MAX_PAYLOAD 1400u     // keep below typical MTU minus IP/UDP

typedef enum {
    RWP_TYPE_CONTROL  = 0,
    RWP_TYPE_VOICE    = 1,
    RWP_TYPE_POSITION = 2,
    RWP_TYPE_EVENT    = 3,
    RWP_TYPE_MAX_     = 3,
} rwp_type_t;

typedef enum {
    RWP_CODEC_IMA_ADPCM = 0,   // 16 kHz / 20 ms, P0
    RWP_CODEC_OPUS      = 1,
    RWP_CODEC_MAX_      = 1,
} rwp_codec_t;

typedef enum {
    RWP_TARGET_GROUP    = 0,
    RWP_TARGET_NODE     = 1,
    RWP_TARGET_SUBGROUP = 2,
    RWP_TARGET_MAX_     = 2,
} rwp_target_t;

enum {
    RWP_FLAG_ENCRYPTED      = 1u << 0,
    RWP_FLAG_START          = 1u << 1,
    RWP_FLAG_END            = 1u << 2,
    RWP_FLAG_FEC            = 1u << 3,
    RWP_FLAG_RECORDING_HINT = 1u << 4,
};

typedef struct {
    uint8_t  version;       // RWP_VERSION
    uint8_t  type;          // rwp_type_t
    uint8_t  codec;         // rwp_codec_t (VOICE only; 0 otherwise)
    uint16_t flags;         // RWP_FLAG_*
    uint16_t header_len;    // >= RWP_HEADER_LEN; extra bytes are skipped by decoders
    uint32_t group_id;
    uint32_t sender_id;
    uint8_t  target_type;   // rwp_target_t
    uint32_t target_id;     // 0 for GROUP
    uint32_t stream_id;     // new value per PTT session
    uint32_t sequence;
    uint32_t capture_time;  // monotonic ms modulo 2^32
    uint16_t payload_len;
} rwp_header_t;

typedef enum {
    RWP_OK               = 0,
    RWP_ERR_ARG          = -1,   // NULL / capacity too small
    RWP_ERR_SHORT        = -2,   // buffer shorter than header
    RWP_ERR_MAGIC        = -3,
    RWP_ERR_VERSION      = -4,
    RWP_ERR_HEADER_LEN   = -5,   // header_len < 36 or > buffer
    RWP_ERR_PAYLOAD_LEN  = -6,   // payload_len exceeds buffer or RWP_MAX_PAYLOAD
    RWP_ERR_FIELD        = -7,   // type / codec / target_type out of range
} rwp_err_t;

// Serialize header + payload into out. Returns total bytes written, or rwp_err_t (<0).
// h->header_len is forced to RWP_HEADER_LEN and h->payload_len to payload_len.
int rwp_encode(uint8_t *out, size_t cap, const rwp_header_t *h,
               const void *payload, size_t payload_len);

// Parse a datagram. On RWP_OK fills *h and sets *payload to point inside `in`.
// Never reads beyond `len`. Unknown flag bits are preserved, not rejected.
int rwp_decode(const uint8_t *in, size_t len, rwp_header_t *h, const uint8_t **payload);

// Wrap-safe sequence comparison: true if a is after b (RFC 1982 style, 32-bit).
bool rwp_seq_after(uint32_t a, uint32_t b);

// ---- CONTROL payload (docs §6 floor control) ----
typedef enum {
    RWP_CTRL_FLOOR_REQUEST = 1,
    RWP_CTRL_FLOOR_GRANT   = 2,
    RWP_CTRL_FLOOR_DENY    = 3,
    RWP_CTRL_FLOOR_RENEW   = 4,   // keep-alive when no voice packet was sent within renew interval
    RWP_CTRL_PTT_END       = 5,
    RWP_CTRL_MAX_          = 5,
} rwp_ctrl_t;

#define RWP_CONTROL_LEN 9u

typedef struct {
    uint8_t  ctrl;        // rwp_ctrl_t
    uint32_t stream_id;   // stream this control refers to
    uint32_t lease_ms;    // GRANT: lease duration; others: 0
} rwp_control_t;

int rwp_control_encode(uint8_t *out, size_t cap, const rwp_control_t *c);
int rwp_control_decode(const uint8_t *in, size_t len, rwp_control_t *c);

#ifdef __cplusplus
}
#endif
