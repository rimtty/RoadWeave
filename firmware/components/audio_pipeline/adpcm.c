#include "adpcm.h"

static const int16_t step_table[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};
static const int8_t index_table[16] = { -1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8 };

void adpcm_state_init(adpcm_state_t *s) { s->predictor = 0; s->step_index = 0; }

static int clamp_index(int i) { return i < 0 ? 0 : (i > 88 ? 88 : i); }

uint8_t adpcm_encode_sample(adpcm_state_t *s, int16_t sample)
{
    int step = step_table[s->step_index];
    int diff = (int)sample - (int)s->predictor;
    uint8_t code = 0;
    if (diff < 0) { code = 8; diff = -diff; }
    int delta = step >> 3;
    if (diff >= step)        { code |= 4; diff -= step; delta += step; }
    step >>= 1;
    if (diff >= step)        { code |= 2; diff -= step; delta += step; }
    step >>= 1;
    if (diff >= step)        { code |= 1; delta += step; }
    int pred = (code & 8) ? (int)s->predictor - delta : (int)s->predictor + delta;
    if (pred > 32767) pred = 32767;
    if (pred < -32768) pred = -32768;
    s->predictor = (int16_t)pred;
    s->step_index = (uint8_t)clamp_index((int)s->step_index + index_table[code]);
    return code;
}

int16_t adpcm_decode_sample(adpcm_state_t *s, uint8_t nibble)
{
    nibble &= 0x0F;
    int step = step_table[s->step_index];
    int delta = step >> 3;
    if (nibble & 4) delta += step;
    if (nibble & 2) delta += step >> 1;
    if (nibble & 1) delta += step >> 2;
    int pred = (nibble & 8) ? (int)s->predictor - delta : (int)s->predictor + delta;
    if (pred > 32767) pred = 32767;
    if (pred < -32768) pred = -32768;
    s->predictor = (int16_t)pred;
    s->step_index = (uint8_t)clamp_index((int)s->step_index + index_table[nibble]);
    return s->predictor;
}

size_t adpcm_encode_block(adpcm_state_t *s, const int16_t *pcm, size_t n, uint8_t *out, size_t cap)
{
    if (!s || !pcm || !out || (n & 1) || n == 0) return 0;
    size_t need = adpcm_block_size(n);
    if (cap < need) return 0;
    out[0] = (uint8_t)((uint16_t)s->predictor >> 8);
    out[1] = (uint8_t)((uint16_t)s->predictor & 0xFF);
    out[2] = s->step_index;
    out[3] = 0;
    uint8_t *p = out + ADPCM_BLOCK_HEADER_LEN;
    for (size_t i = 0; i < n; i += 2) {
        uint8_t lo = adpcm_encode_sample(s, pcm[i]);
        uint8_t hi = adpcm_encode_sample(s, pcm[i + 1]);
        *p++ = (uint8_t)(lo | (hi << 4));
    }
    return need;
}

size_t adpcm_decode_block(const uint8_t *in, size_t len, int16_t *pcm, size_t cap_samples)
{
    if (!in || !pcm || len <= ADPCM_BLOCK_HEADER_LEN) return 0;
    if (in[2] > 88) return 0;
    size_t n = (len - ADPCM_BLOCK_HEADER_LEN) * 2;
    if (cap_samples < n) return 0;
    adpcm_state_t s;
    s.predictor = (int16_t)(((uint16_t)in[0] << 8) | in[1]);
    s.step_index = in[2];
    const uint8_t *p = in + ADPCM_BLOCK_HEADER_LEN;
    for (size_t i = 0; i < n; i += 2) {
        pcm[i]     = adpcm_decode_sample(&s, (uint8_t)(*p & 0x0F));
        pcm[i + 1] = adpcm_decode_sample(&s, (uint8_t)(*p >> 4));
        p++;
    }
    return n;
}
