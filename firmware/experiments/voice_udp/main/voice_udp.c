// RoadWeave P0-B prep: end-to-end voice over UDP with the XIAO's 2.4 GHz Wi-Fi as a
// stand-in for HaLow. Same code path as the product (I2S -> ADPCM -> RWP/UDP ->
// jitter buffer -> decode -> I2S). Peer is tools/rwp_peer.py on a Mac (echo / record / send).
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "lwip/sockets.h"
#include "rwp.h"
#include "adpcm.h"
#include "jitter.h"
#include "sdkconfig.h"

#define FS 16000
#define BLOCK_MS 20
#define BLOCK_SAMPLES (FS * BLOCK_MS / 1000)

static const char *TAG = "voice_udp";
static i2s_chan_handle_t s_rx, s_tx;
static int s_sock = -1;
static struct sockaddr_in s_peer;
static jb_t s_jb;
static adpcm_state_t s_enc;
static SemaphoreHandle_t s_jb_lock;

static volatile uint32_t st_tx = 0, st_rx = 0, st_rx_bad = 0, st_echo = 0;
static volatile int64_t st_rtt_sum = 0, st_rtt_max = 0;
static volatile int64_t st_m2e_sum = 0; static volatile uint32_t st_m2e_n = 0;

static uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

// ---------------- Wi-Fi ----------------
static EventGroupHandle_t s_ev; enum { EV_GOT_IP = 1 };
static void wifi_evt(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) esp_wifi_connect();
    else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) { ESP_LOGW(TAG, "wifi disconnected, retrying"); vTaskDelay(pdMS_TO_TICKS(1000)); esp_wifi_connect(); }
    else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = data; ESP_LOGI(TAG, "got ip " IPSTR, IP2STR(&e->ip_info.ip)); xEventGroupSetBits(s_ev, EV_GOT_IP);
    }
}
static void wifi_start(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init()); ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT(); ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_evt, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_evt, NULL));
    wifi_config_t wc = {0};
    strncpy((char *)wc.sta.ssid, CONFIG_RW_WIFI_SSID, sizeof wc.sta.ssid - 1);
    strncpy((char *)wc.sta.password, CONFIG_RW_WIFI_PASS, sizeof wc.sta.password - 1);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA)); ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));   // latency first
    ESP_ERROR_CHECK(esp_wifi_start());
    s_ev = xEventGroupCreate();
    xEventGroupWaitBits(s_ev, EV_GOT_IP, pdFALSE, pdTRUE, portMAX_DELAY);
}

// ---------------- audio ----------------
static void i2s_start(void)
{
    i2s_chan_config_t chan = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan.dma_desc_num = 6; chan.dma_frame_num = BLOCK_SAMPLES; chan.auto_clear = true;
#if CONFIG_RW_I2S_SPK_DOUT_GPIO >= 0
    ESP_ERROR_CHECK(i2s_new_channel(&chan, &s_tx, &s_rx));
#else
    ESP_ERROR_CHECK(i2s_new_channel(&chan, NULL, &s_rx));
#endif
    i2s_std_config_t cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(FS),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = { .mclk = I2S_GPIO_UNUSED, .bclk = CONFIG_RW_I2S_BCLK_GPIO, .ws = CONFIG_RW_I2S_WS_GPIO,
                      .dout = CONFIG_RW_I2S_SPK_DOUT_GPIO >= 0 ? CONFIG_RW_I2S_SPK_DOUT_GPIO : I2S_GPIO_UNUSED,
                      .din = CONFIG_RW_I2S_MIC_DIN_GPIO },
    };
    cfg.slot_cfg.bit_shift = true;
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_rx, &cfg));
    if (s_tx) { ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_tx, &cfg)); ESP_ERROR_CHECK(i2s_channel_enable(s_tx)); }
    ESP_ERROR_CHECK(i2s_channel_enable(s_rx));
#if CONFIG_RW_PTT_GPIO >= 0
    gpio_config_t ptt = { .pin_bit_mask = 1ULL << CONFIG_RW_PTT_GPIO, .mode = GPIO_MODE_INPUT, .pull_up_en = GPIO_PULLUP_ENABLE };
    ESP_ERROR_CHECK(gpio_config(&ptt));
#endif
#if CONFIG_RW_AMP_SD_GPIO >= 0
    gpio_config_t sd = { .pin_bit_mask = 1ULL << CONFIG_RW_AMP_SD_GPIO, .mode = GPIO_MODE_OUTPUT };
    ESP_ERROR_CHECK(gpio_config(&sd)); gpio_set_level(CONFIG_RW_AMP_SD_GPIO, 1);
#endif
}

static bool ptt_pressed(void)
{
#if CONFIG_RW_TX_ALWAYS
    return true;
#elif CONFIG_RW_PTT_GPIO >= 0
    return gpio_get_level(CONFIG_RW_PTT_GPIO) == 0;
#else
    return false;
#endif
}

static int16_t hpf(int16_t x)
{
    static int32_t px = 0, py = 0; const int32_t a = 31470;
    int32_t y = x - px + ((a * py) >> 15); px = x; py = y;
    if (y > 32767) y = 32767;
    if (y < -32768) y = -32768;
    return (int16_t)y;
}

// ---------------- RX task: socket -> jitter buffer ----------------
static void rx_task(void *arg)
{
    static uint8_t buf[1500];
    for (;;) {
        struct sockaddr_in from; socklen_t fl = sizeof from;
        int n = recvfrom(s_sock, buf, sizeof buf, 0, (struct sockaddr *)&from, &fl);
        if (n <= 0) continue;
        rwp_header_t h; const uint8_t *pl;
        if (rwp_decode(buf, (size_t)n, &h, &pl) != RWP_OK) { st_rx_bad++; continue; }
        if (h.type != RWP_TYPE_VOICE || h.codec != RWP_CODEC_IMA_ADPCM) continue;
        st_rx++;
        bool echo = (h.sender_id == CONFIG_RW_SENDER_ID);
        if (echo) {
            int64_t rtt = (int32_t)(now_ms() - h.capture_time);
            st_echo++; st_rtt_sum += rtt; if (rtt > st_rtt_max) st_rtt_max = rtt;
#if !CONFIG_RW_PLAY_ECHO
            continue;
#endif
        }
        xSemaphoreTake(s_jb_lock, portMAX_DELAY);
        // tag = capture_time for echoed frames (own clock) so playout can measure mouth-to-ear incl. buffering
        jb_put_tag(&s_jb, h.stream_id ^ (h.sender_id << 8), h.sequence, pl, h.payload_len, echo ? h.capture_time : 0, now_ms());
        xSemaphoreGive(s_jb_lock);
    }
}

// ---------------- audio task: I2S -> encode -> UDP, jitter -> decode -> I2S ----------------
static void audio_task(void *arg)
{
    static int32_t rxbuf[BLOCK_SAMPLES * 2], txbuf[BLOCK_SAMPLES * 2];
    static int16_t pcm[BLOCK_SAMPLES], out[BLOCK_SAMPLES];
    static uint8_t pkt[RWP_HEADER_LEN + 200], frame[JB_MAX_PAYLOAD];
    uint32_t stream_id = 0, seq = 0; bool was_tx = false;
    uint32_t last_report = now_ms();

    for (;;) {
        size_t got = 0;
        if (i2s_channel_read(s_rx, rxbuf, sizeof rxbuf, &got, 100) != ESP_OK || got != sizeof rxbuf) continue;
        uint32_t t_cap = now_ms();
        for (int i = 0; i < BLOCK_SAMPLES; i++) pcm[i] = hpf((int16_t)(rxbuf[2 * i] >> 16));

        bool tx = ptt_pressed();
        if (tx && !was_tx) { stream_id = t_cap ? t_cap : 1; seq = 0; adpcm_state_init(&s_enc); }
        if (tx) {
            rwp_header_t h = { .version = RWP_VERSION, .type = RWP_TYPE_VOICE, .codec = RWP_CODEC_IMA_ADPCM,
                .flags = (uint16_t)(seq == 0 ? RWP_FLAG_START : 0), .group_id = CONFIG_RW_GROUP_ID,
                .sender_id = CONFIG_RW_SENDER_ID, .target_type = RWP_TARGET_GROUP, .target_id = 0,
                .stream_id = stream_id, .sequence = seq++, .capture_time = t_cap };
            uint8_t blk[164]; size_t bl = adpcm_encode_block(&s_enc, pcm, BLOCK_SAMPLES, blk, sizeof blk);
            int n = rwp_encode(pkt, sizeof pkt, &h, blk, bl);
            if (n > 0 && sendto(s_sock, pkt, (size_t)n, 0, (struct sockaddr *)&s_peer, sizeof s_peer) == n) st_tx++;
        }
        was_tx = tx;

        // playout: one frame per 20 ms block, clocked by the I2S read
        size_t fl = 0; bool have = false; uint32_t tag = 0;
        xSemaphoreTake(s_jb_lock, portMAX_DELAY);
        jb_get_result_t r = jb_get_tag(&s_jb, frame, sizeof frame, &fl, &tag, now_ms());
        xSemaphoreGive(s_jb_lock);
        if (r == JB_GET_FRAME && adpcm_decode_block(frame, fl, out, BLOCK_SAMPLES) == BLOCK_SAMPLES) {
            have = true;
            if (tag) { st_m2e_sum += (int32_t)(now_ms() - tag); st_m2e_n++; }   // capture -> this playout call
        }
        if (s_tx) {
            for (int i = 0; i < BLOCK_SAMPLES; i++) {
                int32_t v = have && !(tx && !CONFIG_RW_TX_ALWAYS) ? out[i] * CONFIG_RW_OUTPUT_GAIN_PERCENT / 100 : 0;
                if (v > 32767) v = 32767;
                if (v < -32768) v = -32768;
                txbuf[2 * i] = v << 16; txbuf[2 * i + 1] = v << 16;
            }
            size_t w; i2s_channel_write(s_tx, txbuf, sizeof txbuf, &w, 100);
        }

        if ((uint32_t)(now_ms() - last_report) >= 1000) {
            last_report = now_ms();
            const jb_stats_t *js = jb_stats(&s_jb);
            static uint32_t p_echo = 0, p_m2e_n = 0; static int64_t p_rtt = 0, p_m2e = 0;
            uint32_t d_echo = st_echo - p_echo, d_m2e_n = st_m2e_n - p_m2e_n;
            int64_t d_rtt = st_rtt_sum - p_rtt, d_m2e = st_m2e_sum - p_m2e;
            printf("tx %lu rx %lu bad %lu | 1s: echo %lu rtt avg %lld max %lld ms | mouth-to-ear avg %lld ms | jb depth %u played %lu gap %lu late %lu underrun %lu grow %lu shrink %lu\n",
                   (unsigned long)st_tx, (unsigned long)st_rx, (unsigned long)st_rx_bad, (unsigned long)d_echo,
                   d_echo ? (long long)(d_rtt / d_echo) : 0LL, (long long)st_rtt_max,
                   d_m2e_n ? (long long)(d_m2e / d_m2e_n) : 0LL,
                   js->depth_ms, (unsigned long)js->frames_played, (unsigned long)js->gap, (unsigned long)js->late,
                   (unsigned long)js->underrun, (unsigned long)js->grow, (unsigned long)js->shrink);
            p_echo = st_echo; p_m2e_n = st_m2e_n; p_rtt = st_rtt_sum; p_m2e = st_m2e_sum;
            st_rtt_max = 0;
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "voice_udp: peer %s:%d sender 0x%08x group 0x%08x tx_always=%d",
             CONFIG_RW_PEER_IP, CONFIG_RW_UDP_PORT, CONFIG_RW_SENDER_ID, CONFIG_RW_GROUP_ID, CONFIG_RW_TX_ALWAYS);
    if (strlen(CONFIG_RW_WIFI_SSID) == 0) { ESP_LOGE(TAG, "set RW_WIFI_SSID / RW_WIFI_PASS via idf.py menuconfig"); return; }
    wifi_start();

    s_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in me = { .sin_family = AF_INET, .sin_port = htons(CONFIG_RW_UDP_PORT), .sin_addr.s_addr = htonl(INADDR_ANY) };
    ESP_ERROR_CHECK(bind(s_sock, (struct sockaddr *)&me, sizeof me) == 0 ? ESP_OK : ESP_FAIL);
    s_peer.sin_family = AF_INET; s_peer.sin_port = htons(CONFIG_RW_UDP_PORT); s_peer.sin_addr.s_addr = inet_addr(CONFIG_RW_PEER_IP);

    s_jb_lock = xSemaphoreCreateMutex();
    jb_init(&s_jb, NULL);
    adpcm_state_init(&s_enc);
    i2s_start();
    xTaskCreatePinnedToCore(rx_task, "rx", 4096, NULL, 6, NULL, 0);
    xTaskCreatePinnedToCore(audio_task, "audio", 8192, NULL, 7, NULL, 0);
}
