// RoadWeave P0-B audio bench (Issue #4 / #18)
// - I2S full-duplex on I2S0: MEMS mic (SPH0645 / INMP441) RX, MAX98357A TX, shared BCLK/WS
// - 16 kHz mono, 20 ms blocks, 32-bit slots (mic delivers 18/24-bit MSB-justified)
// - Prints level meter (RMS/peak/DC), block processing time, over/underrun counters
// - PTT pressed = TX state -> speaker hard mute (design rule: no self-monitoring while talking)
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "sdkconfig.h"

#define FS            16000
#define BLOCK_MS      20
#define BLOCK_SAMPLES (FS * BLOCK_MS / 1000)   // 320
#define SLOT_BITS     32

#ifdef CONFIG_RW_TEST_TONE
#define RW_TEST_TONE 1
#else
#define RW_TEST_TONE 0
#endif
#ifdef CONFIG_RW_LOOPBACK
#define RW_LOOPBACK 1
#else
#define RW_LOOPBACK 0
#endif

static const char *TAG = "audio_bench";

static i2s_chan_handle_t s_rx, s_tx;
static volatile uint32_t s_rx_overflow, s_tx_underflow;
static int32_t s_rx_buf[BLOCK_SAMPLES * 2];  // stereo slots (left = mic, right = unused)
static int32_t s_tx_buf[BLOCK_SAMPLES * 2];

static bool IRAM_ATTR on_rx_ovf(i2s_chan_handle_t h, i2s_event_data_t *e, void *ctx) { s_rx_overflow++; return false; }
static bool IRAM_ATTR on_tx_udf(i2s_chan_handle_t h, i2s_event_data_t *e, void *ctx) { s_tx_underflow++; return false; }

static bool ptt_pressed(void)
{
#if CONFIG_RW_PTT_GPIO >= 0
    return gpio_get_level(CONFIG_RW_PTT_GPIO) == 0;
#else
    return false;
#endif
}

static void amp_enable(bool on)
{
#if CONFIG_RW_AMP_SD_GPIO >= 0
    gpio_set_level(CONFIG_RW_AMP_SD_GPIO, on ? 1 : 0);
#else
    (void)on;
#endif
}

static void init_gpio(void)
{
#if CONFIG_RW_PTT_GPIO >= 0
    gpio_config_t ptt = { .pin_bit_mask = 1ULL << CONFIG_RW_PTT_GPIO, .mode = GPIO_MODE_INPUT,
                          .pull_up_en = GPIO_PULLUP_ENABLE };
    ESP_ERROR_CHECK(gpio_config(&ptt));
#endif
#if CONFIG_RW_AMP_SD_GPIO >= 0
    gpio_config_t sd = { .pin_bit_mask = 1ULL << CONFIG_RW_AMP_SD_GPIO, .mode = GPIO_MODE_OUTPUT };
    ESP_ERROR_CHECK(gpio_config(&sd));
    amp_enable(false);   // stay muted until I2S clocks are stable (pop suppression)
#endif
}

static void init_i2s(void)
{
    i2s_chan_config_t chan = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan.dma_desc_num = 6;
    chan.dma_frame_num = BLOCK_SAMPLES;
    chan.auto_clear = true;   // TX sends zeros on underrun instead of stale data
#if CONFIG_RW_I2S_SPK_DOUT_GPIO >= 0
    ESP_ERROR_CHECK(i2s_new_channel(&chan, &s_tx, &s_rx));
#else
    ESP_ERROR_CHECK(i2s_new_channel(&chan, NULL, &s_rx));
#endif

    i2s_std_config_t cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(FS),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = CONFIG_RW_I2S_BCLK_GPIO,
            .ws   = CONFIG_RW_I2S_WS_GPIO,
            .dout = CONFIG_RW_I2S_SPK_DOUT_GPIO >= 0 ? CONFIG_RW_I2S_SPK_DOUT_GPIO : I2S_GPIO_UNUSED,
            .din  = CONFIG_RW_I2S_MIC_DIN_GPIO,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };
    // Philips: data starts 1 BCLK after WS edge, which matches SPH0645/INMP441 timing.
    cfg.slot_cfg.bit_shift = true;
    cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_rx, &cfg));
    i2s_event_callbacks_t rx_cb = { .on_recv_q_ovf = on_rx_ovf };
    ESP_ERROR_CHECK(i2s_channel_register_event_callback(s_rx, &rx_cb, NULL));
    if (s_tx) {
        ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_tx, &cfg));
        i2s_event_callbacks_t tx_cb = { .on_send_q_ovf = on_tx_udf };
        ESP_ERROR_CHECK(i2s_channel_register_event_callback(s_tx, &tx_cb, NULL));
        memset(s_tx_buf, 0, sizeof s_tx_buf);
        size_t w;
        for (int i = 0; i < 3; i++) i2s_channel_preload_data(s_tx, s_tx_buf, sizeof s_tx_buf, &w);
        ESP_ERROR_CHECK(i2s_channel_enable(s_tx));
    }
    ESP_ERROR_CHECK(i2s_channel_enable(s_rx));
    vTaskDelay(pdMS_TO_TICKS(50));
    amp_enable(true);
}

// 1st-order high-pass, fc ~ 100 Hz @ 16 kHz, on int32 with Q15 coefficient
static int16_t hpf(int16_t x)
{
    static int32_t prev_x = 0, prev_y = 0;
    const int32_t a = 31470;  // exp(-2*pi*100/16000) in Q15 ~ 0.9614
    int32_t y = x - prev_x + ((a * prev_y) >> 15);
    prev_x = x; prev_y = y;
    if (y > 32767) y = 32767;
    if (y < -32768) y = -32768;
    return (int16_t)y;
}

void app_main(void)
{
    ESP_LOGI(TAG, "RoadWeave audio bench: %d Hz, %d ms blocks, BCLK=%d WS=%d MIC_DIN=%d SPK_DOUT=%d PTT=%d AMP_SD=%d",
             FS, BLOCK_MS, CONFIG_RW_I2S_BCLK_GPIO, CONFIG_RW_I2S_WS_GPIO, CONFIG_RW_I2S_MIC_DIN_GPIO,
             CONFIG_RW_I2S_SPK_DOUT_GPIO, CONFIG_RW_PTT_GPIO, CONFIG_RW_AMP_SD_GPIO);
    ESP_LOGI(TAG, "loopback=%d test_tone=%d gain=%d%%", RW_LOOPBACK, RW_TEST_TONE, CONFIG_RW_OUTPUT_GAIN_PERCENT);
    init_gpio();
    init_i2s();

    int16_t pcm[BLOCK_SAMPLES];
    uint32_t blocks = 0; int64_t proc_us_sum = 0, proc_us_max = 0;
    double rms_acc = 0; int32_t peak = 0; int64_t dc_acc = 0; uint32_t acc_n = 0;
    bool was_tx = false;
    float tone_phase = 0.f;

    for (;;) {
        size_t got = 0;
        esp_err_t r = i2s_channel_read(s_rx, s_rx_buf, sizeof s_rx_buf, &got, 100 /* ms */);
        if (r != ESP_OK || got != sizeof s_rx_buf) { ESP_LOGW(TAG, "i2s read %s got=%u", esp_err_to_name(r), (unsigned)got); continue; }
        int64_t t0 = esp_timer_get_time();

        // left slot only; mic data is MSB-justified in the 32-bit slot -> take top 16 bits
        int32_t dc = 0;
        for (int i = 0; i < BLOCK_SAMPLES; i++) {
            int32_t raw = s_rx_buf[2 * i] >> 16;
            dc += raw;
            pcm[i] = hpf((int16_t)raw);
        }
        dc_acc += dc / BLOCK_SAMPLES;
        double sq = 0;
        for (int i = 0; i < BLOCK_SAMPLES; i++) { int32_t v = pcm[i]; sq += (double)v * v; if (v < 0) v = -v; if (v > peak) peak = v; }
        rms_acc += sq / BLOCK_SAMPLES; acc_n++;

        bool tx = ptt_pressed();
        if (tx != was_tx) {
            amp_enable(!tx);
            if (tx) ESP_LOGI(TAG, "PTT down: TX state, speaker hard mute");
            else    ESP_LOGI(TAG, "PTT up: RX state");
            was_tx = tx;
        }

        if (s_tx) {
            for (int i = 0; i < BLOCK_SAMPLES; i++) {
                int32_t v = 0;
                if (!tx) {
#if RW_TEST_TONE
                    v = (int32_t)(8000.f * sinf(tone_phase)); tone_phase += 2.f * (float)M_PI * 1000.f / FS;
                    if (tone_phase > 2.f * (float)M_PI) tone_phase -= 2.f * (float)M_PI;
#elif RW_LOOPBACK
                    v = pcm[i];
#endif
                    v = v * CONFIG_RW_OUTPUT_GAIN_PERCENT / 100;
                    if (v > 32767) v = 32767;
                    if (v < -32768) v = -32768;
                }
                s_tx_buf[2 * i] = v << 16; s_tx_buf[2 * i + 1] = v << 16;
            }
            size_t w = 0;
            i2s_channel_write(s_tx, s_tx_buf, sizeof s_tx_buf, &w, 100 /* ms */);
        }

        int64_t dt = esp_timer_get_time() - t0;
        proc_us_sum += dt; if (dt > proc_us_max) proc_us_max = dt;
        if (++blocks % 25 == 0) {   // every 500 ms
            double rms = sqrt(rms_acc / acc_n);
            double dbfs = rms > 0 ? 20.0 * log10(rms / 32768.0) : -120.0;
            int bars = (int)((dbfs + 60.0) / 60.0 * 20.0); if (bars < 0) bars = 0; if (bars > 20) bars = 20;
            char meter[22]; memset(meter, '#', (size_t)bars); memset(meter + bars, '.', (size_t)(20 - bars)); meter[20] = 0;
            printf("[%s] rms %6.1f dBFS peak %5ld dc %6lld | proc avg %4lld us max %4lld us (%2.0f%% of %d ms) | rx_ovf %lu tx_udf %lu | %s\n",
                   meter, dbfs, (long)peak, (long long)(dc_acc / acc_n), (long long)(proc_us_sum / 25), (long long)proc_us_max,
                   100.0 * (double)proc_us_max / (BLOCK_MS * 1000.0), BLOCK_MS,
                   (unsigned long)s_rx_overflow, (unsigned long)s_tx_underflow, tx ? "TX(mute)" : "RX");
            rms_acc = 0; peak = 0; dc_acc = 0; acc_n = 0; proc_us_sum = 0; proc_us_max = 0;
        }
    }
}
