// Opus CPU/RAM benchmark on ESP32-S3 (Issue #14 前倒し).
// 16 kHz mono, synthetic speech-like signal, encode+decode per frame timing.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "opus.h"

#define FS          16000
#define CHANNELS    1
#define MAX_FRAME   (FS * 60 / 1000)   // 60 ms max
#define TEST_MS     3000               // signal length per case

static int16_t *g_pcm;                 // TEST_MS of synthetic audio
static uint8_t g_packet[1500];
static int16_t g_out[MAX_FRAME];

static void synth(int16_t *pcm, int n)
{
    // pseudo-speech: two formant-ish tones with 4 Hz syllable AM + noise floor
    uint32_t x = 1;
    for (int i = 0; i < n; i++) {
        double t = (double)i / FS;
        double am = 0.55 + 0.45 * sin(2 * M_PI * 4.0 * t);
        double s = 0.5 * sin(2 * M_PI * 220 * t) + 0.3 * sin(2 * M_PI * 660 * t) + 0.15 * sin(2 * M_PI * 1800 * t);
        x = x * 1664525u + 1013904223u;
        double noise = ((int32_t)(x >> 8) / 8388608.0 - 1.0) * 0.02;
        pcm[i] = (int16_t)(12000.0 * am * s + 3000.0 * noise);
    }
}

typedef struct { int bitrate; int frame_ms; int complexity; } bench_case_t;

static void run_case(const bench_case_t *c, int decoders)
{
    int err = 0;
    size_t heap_before = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    // 状態は明示的に内部RAMへ置く。malloc任せだと16 KiB超はPSRAMへ回り、遅くなる。
    OpusEncoder *enc = heap_caps_malloc((size_t)opus_encoder_get_size(CHANNELS), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!enc || (err = opus_encoder_init(enc, FS, CHANNELS, OPUS_APPLICATION_VOIP)) != OPUS_OK) {
        printf("encoder init failed: %d\n", err); return;
    }
    OpusDecoder *dec[8] = {0};
    for (int d = 0; d < decoders; d++) {
        dec[d] = heap_caps_malloc((size_t)opus_decoder_get_size(CHANNELS), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (!dec[d] || opus_decoder_init(dec[d], FS, CHANNELS) != OPUS_OK) { printf("decoder init failed\n"); return; }
    }
    size_t heap_after = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);

    opus_encoder_ctl(enc, OPUS_SET_BITRATE(c->bitrate));
    opus_encoder_ctl(enc, OPUS_SET_COMPLEXITY(c->complexity));
    opus_encoder_ctl(enc, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
    opus_encoder_ctl(enc, OPUS_SET_VBR(1));
    opus_encoder_ctl(enc, OPUS_SET_INBAND_FEC(0));
    opus_encoder_ctl(enc, OPUS_SET_DTX(0));

    int frame = FS * c->frame_ms / 1000;
    int frames = TEST_MS / c->frame_ms;
    int64_t enc_us = 0, dec_us = 0, enc_max = 0, dec_max = 0;
    size_t bytes = 0;

    for (int f = 0; f < frames; f++) {
        int64_t t0 = esp_timer_get_time();
        int n = opus_encode(enc, g_pcm + (size_t)f * frame, frame, g_packet, sizeof g_packet);
        int64_t t1 = esp_timer_get_time();
        if (n < 0) { printf("encode error %d\n", n); break; }
        bytes += (size_t)n;
        enc_us += t1 - t0; if (t1 - t0 > enc_max) enc_max = t1 - t0;
        for (int d = 0; d < decoders; d++) {
            int64_t t2 = esp_timer_get_time();
            int m = opus_decode(dec[d], g_packet, n, g_out, MAX_FRAME, 0);
            int64_t t3 = esp_timer_get_time();
            if (m != frame) { printf("decode error %d\n", m); break; }
            dec_us += t3 - t2; if (t3 - t2 > dec_max) dec_max = t3 - t2;
        }
    }
    double enc_avg = (double)enc_us / frames;
    double dec_avg = (double)dec_us / (frames * decoders);
    double frame_us = c->frame_ms * 1000.0;
    printf("| %5d | %2d ms | c%d | %2d | %7.0f us (%4.1f%%) | %6lld us | %7.0f us (%4.1f%%) | %6lld us | %4.1f%% | %5.1f kbps | %6u B |\n",
           c->bitrate, c->frame_ms, c->complexity, decoders,
           enc_avg, 100.0 * enc_avg / frame_us, (long long)enc_max,
           dec_avg, 100.0 * dec_avg / frame_us, (long long)dec_max,
           100.0 * (enc_avg + dec_avg * decoders) / frame_us,
           8.0 * bytes / (TEST_MS / 1000.0) / 1000.0,
           (unsigned)(heap_before - heap_after));

    for (int d = 0; d < decoders; d++) heap_caps_free(dec[d]);
    heap_caps_free(enc);
}

void app_main(void)
{
    printf("\n=== Opus benchmark: %s, ESP32-S3 @ %d MHz, fixed-point, -O2, states in internal RAM ===\n",
           opus_get_version_string(), CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ);
    printf("encoder state %d B, decoder state %d B (16 kHz mono)\n",
           opus_encoder_get_size(CHANNELS), opus_decoder_get_size(CHANNELS));
    g_pcm = heap_caps_malloc(sizeof(int16_t) * FS * TEST_MS / 1000, MALLOC_CAP_SPIRAM);
    if (!g_pcm) { printf("no PSRAM for test signal\n"); return; }
    synth(g_pcm, FS * TEST_MS / 1000);
    printf("free internal heap before: %u B\n", (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    printf("| kbps  | frame | cx | dec | encode avg (CPU) | enc max | decode avg (CPU) | dec max | total | actual  | heap   |\n");
    printf("|-------|-------|----|-----|------------------|---------|------------------|---------|-------|---------|--------|\n");
    const bench_case_t cases[] = {
        { 8000, 20, 0 }, {12000, 20, 0 }, {16000, 20, 0 },
        { 8000, 20, 3 }, {12000, 20, 3 }, {16000, 20, 3 },
        {12000, 20, 5 }, {12000, 40, 0 }, {12000, 40, 3 },
        {24000, 20, 3 },
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        run_case(&cases[i], 1);
        vTaskDelay(1);
    }
    printf("--- multiple simultaneous decoders (voice room): 1 encoder + N decoders ---\n");
    const bench_case_t room[] = { {12000, 20, 0}, {12000, 20, 3} };
    for (int c = 0; c < 2; c++) for (int n = 2; n <= 6; n += 2) { run_case(&room[c], n); vTaskDelay(1); }
    printf("free internal heap after: %u B\n", (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    printf("=== OPUS_BENCH_DONE ===\n");
}
