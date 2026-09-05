// IMA ADPCM (DVI4-style) for RoadWeave P0 voice: 16 kHz mono, 4 bit/sample.
// Each packet block carries its own predictor state so a lost packet never
// corrupts the next one (decoder resyncs per block). Pure C11, no ESP-IDF.
#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ADPCM_BLOCK_HEADER_LEN 4u   // int16 predictor (BE), uint8 step index, uint8 reserved(0)

typedef struct {
    int16_t predictor;
    uint8_t step_index;   // 0..88
} adpcm_state_t;

void adpcm_state_init(adpcm_state_t *s);

// Bytes needed to encode n samples as one block (n must be even).
static inline size_t adpcm_block_size(size_t n_samples) { return ADPCM_BLOCK_HEADER_LEN + n_samples / 2; }

// Encode n_samples (even) into out. The encoder state persists across calls so the
// prediction stays continuous; the state *before* this block is written into the header.
// Returns bytes written, or 0 on error (odd n, cap too small).
size_t adpcm_encode_block(adpcm_state_t *s, const int16_t *pcm, size_t n_samples, uint8_t *out, size_t cap);

// Decode one block produced by adpcm_encode_block. Uses the state from the header,
// so blocks can be decoded independently and in any order. Returns samples written,
// or 0 on error (short input, bad step index, out too small).
size_t adpcm_decode_block(const uint8_t *in, size_t len, int16_t *pcm, size_t cap_samples);

// Raw nibble codec (no header) for callers that manage state themselves.
uint8_t adpcm_encode_sample(adpcm_state_t *s, int16_t sample);
int16_t adpcm_decode_sample(adpcm_state_t *s, uint8_t nibble);

#ifdef __cplusplus
}
#endif
