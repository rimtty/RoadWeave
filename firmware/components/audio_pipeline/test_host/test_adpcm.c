#include "test.h"
#include "adpcm.h"
#include <math.h>
#include <string.h>

#define N 320  // 20 ms @ 16 kHz

static double snr_db(const int16_t *ref, const int16_t *out, size_t n)
{
    double s = 0, e = 0;
    for (size_t i = 0; i < n; i++) { double d = (double)ref[i] - out[i]; s += (double)ref[i] * ref[i]; e += d * d; }
    return e == 0 ? 120.0 : 10.0 * log10(s / e);
}

static void test_sizes(void)
{
    CHECK_EQ(adpcm_block_size(N), 4 + 160);
    adpcm_state_t s; adpcm_state_init(&s); int16_t pcm[N] = {0}; uint8_t out[200];
    CHECK_EQ(adpcm_encode_block(&s, pcm, N, out, sizeof out), 164);
    CHECK_EQ(adpcm_encode_block(&s, pcm, N, out, 163), 0);        // cap too small
    CHECK_EQ(adpcm_encode_block(&s, pcm, N - 1, out, sizeof out), 0); // odd
    CHECK_EQ(adpcm_encode_block(&s, pcm, 0, out, sizeof out), 0);
    int16_t dec[N];
    CHECK_EQ(adpcm_decode_block(out, 164, dec, N), N);
    CHECK_EQ(adpcm_decode_block(out, 4, dec, N), 0);              // header only
    CHECK_EQ(adpcm_decode_block(out, 164, dec, N - 1), 0);        // out too small
    uint8_t bad[164]; memcpy(bad, out, 164); bad[2] = 89;
    CHECK_EQ(adpcm_decode_block(bad, 164, dec, N), 0);            // bad step index
}

static void test_silence_and_sine_quality(void)
{
    adpcm_state_t s; adpcm_state_init(&s);
    int16_t pcm[N], dec[N]; uint8_t blk[164];
    memset(pcm, 0, sizeof pcm);
    adpcm_encode_block(&s, pcm, N, blk, sizeof blk); adpcm_decode_block(blk, 164, dec, N);
    int nz = 0; for (int i = 0; i < N; i++) if (dec[i] != 0) nz++;
    CHECK_EQ(nz, 0);
    // 440 Hz at -6 dBFS over 10 consecutive blocks: SNR after the first (attack) block must be decent
    adpcm_state_init(&s);
    double worst = 120;
    for (int b = 0; b < 10; b++) {
        for (int i = 0; i < N; i++) pcm[i] = (int16_t)(16000.0 * sin(2 * M_PI * 440.0 * (b * N + i) / 16000.0));
        adpcm_encode_block(&s, pcm, N, blk, sizeof blk);
        adpcm_decode_block(blk, 164, dec, N);
        double snr = snr_db(pcm, dec, N);
        if (b > 0 && snr < worst) worst = snr;
    }
    CHECK(worst > 20.0);   // IMA ADPCM on a pure tone is typically 25-35 dB
}

static void test_clipping_and_full_scale(void)
{
    adpcm_state_t s; adpcm_state_init(&s);
    int16_t pcm[N], dec[N]; uint8_t blk[164];
    for (int i = 0; i < N; i++) pcm[i] = (i / 8) % 2 ? 32767 : -32768;   // brutal square wave
    for (int b = 0; b < 3; b++) { adpcm_encode_block(&s, pcm, N, blk, sizeof blk); CHECK_EQ(adpcm_decode_block(blk, 164, dec, N), N); }
    for (int i = 0; i < N; i++) CHECK(dec[i] >= -32768 && dec[i] <= 32767);
    CHECK(s.step_index <= 88);
}

static void test_packet_loss_resync(void)
{
    // Encoder runs continuously over blocks 0..4. Decoder receives 0,1,3,4 (2 lost).
    // Because each block carries its state, decoding 3 must give the same output as a
    // decoder that had seen 2.
    adpcm_state_t s; adpcm_state_init(&s);
    int16_t pcm[5][N], dec_all[N], dec_skip[N]; uint8_t blk[5][164];
    uint32_t x = 7;
    for (int b = 0; b < 5; b++) for (int i = 0; i < N; i++) { x = x * 1664525u + 1013904223u; pcm[b][i] = (int16_t)((x >> 16) / 4) - 8192; }
    for (int b = 0; b < 5; b++) adpcm_encode_block(&s, pcm[b], N, blk[b], 164);
    adpcm_decode_block(blk[3], 164, dec_all, N);
    adpcm_decode_block(blk[3], 164, dec_skip, N);   // independent decode, same input
    CHECK(memcmp(dec_all, dec_skip, sizeof dec_all) == 0);
    // header carries the pre-block state: block 1 header == encoder state after block 0
    adpcm_state_t s2; adpcm_state_init(&s2); uint8_t tmp[164];
    adpcm_encode_block(&s2, pcm[0], N, tmp, 164);
    CHECK_EQ((int16_t)((blk[1][0] << 8) | blk[1][1]), s2.predictor);
    CHECK_EQ(blk[1][2], s2.step_index);
}

static void test_known_vector(void)
{
    // Reference nibbles for a tiny ramp, computed by the canonical IMA algorithm.
    adpcm_state_t s; adpcm_state_init(&s);
    const int16_t ramp[8] = { 0, 100, 200, 300, 400, 500, 600, 700 };
    uint8_t blk[8];
    CHECK_EQ(adpcm_encode_block(&s, ramp, 8, blk, sizeof blk), 8);
    CHECK_EQ(blk[0], 0); CHECK_EQ(blk[1], 0); CHECK_EQ(blk[2], 0); CHECK_EQ(blk[3], 0);
    // first sample 0 -> code 0; second 100 with step 7 -> saturates to code 7 (delta 7+3+1+0)
    CHECK_EQ(blk[4] & 0x0F, 0); CHECK_EQ(blk[4] >> 4, 7);
    int16_t dec[8]; adpcm_decode_block(blk, 8, dec, 8);
    CHECK_EQ(dec[0], 0); CHECK_EQ(dec[1], 0 + 7 + 3 + 1 + 0);
    // print vector so tools/rwp_peer.py can cross-check its Python decoder
    printf("vector: ");
    for (int i = 0; i < 8; i++) printf("%02x", blk[i]);
    printf(" -> ");
    for (int i = 0; i < 8; i++) printf("%d ", dec[i]);
    printf("\n");
}

int main(void)
{
    test_sizes(); test_silence_and_sine_quality(); test_clipping_and_full_scale();
    test_packet_loss_resync(); test_known_vector();
    printf("test_adpcm: %d checks, %d failures\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
