/* Uses the APIs demonstrated by morsemicro/halow 2.11.2-esp32-2 softap and
 * sta_connect. Both ends use static IPv4 so this first test is independent of DHCP. */
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include "arpa/inet.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "mmhalow.h"
#include "nvs_flash.h"
#include "link_test.h"

#ifdef CONFIG_HALOW_PS_MODE
#error Wio-WM6180 requires HALOW_PS_MODE disabled because BUSY/WAKE are unwired.
#endif

#define AP_IP "192.168.50.1"
#if defined(CONFIG_RW_LINK_STA_ID) && CONFIG_RW_LINK_STA_ID == 2
#define STA_IP "192.168.50.3"
#else
#define STA_IP "192.168.50.2"
#endif
#define PORT 3333
#define PROBES 20
#define LINK_BIT BIT0
#define IP_BIT BIT1
static EventGroupHandle_t events;
static esp_netif_t *netif;
static struct mmwlan_s1g_channel_list bench_channels;
#ifdef CONFIG_RW_LINK_CONTINUOUS
static bool radio_shutdown_ok;
bool validation_radio_shutdown_ok(void)
{
    return radio_shutdown_ok;
}
#endif
#ifdef CONFIG_RW_LINK_DHCP
static uint32_t dhcp_wait_start_ms;
#ifdef CONFIG_RW_LINK_AP
static portMUX_TYPE lease_lock = portMUX_INITIALIZER_UNLOCKED;
static struct { uint32_t ip; uint8_t mac[6]; } leases[2];
static struct { bool present; uint8_t mac[6]; } authorized[2];
#endif
#endif
#ifdef CONFIG_RW_LINK_CONTINUOUS
bool validation_link_ready(void)
{
    return (xEventGroupGetBits(events) & (LINK_BIT | IP_BIT)) == (LINK_BIT | IP_BIT);
}
bool validation_sta_ip(char *out, size_t out_len)
{
    esp_netif_ip_info_t info = {0};
    if (!validation_link_ready() || esp_netif_get_ip_info(netif, &info) != ESP_OK ||
        info.ip.addr == 0) return false;
    return snprintf(out, out_len, IPSTR, IP2STR(&info.ip)) > 0;
}
bool validation_ap_netif_up(void)
{
    esp_netif_action_connected(netif, NULL, 0, NULL);
#if defined(CONFIG_RW_LINK_DHCP) && defined(CONFIG_RW_LINK_AP)
    esp_err_t err = esp_netif_dhcps_start(netif);
    esp_netif_dhcp_status_t status;
    if ((err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) ||
        esp_netif_dhcps_get_status(netif, &status) != ESP_OK || status != ESP_NETIF_DHCP_STARTED) {
        printf("RW_LINK_DHCP_SERVER=FAIL start_err=%d\n", err);
        return false;
    }
    printf("RW_LINK_DHCP_SERVER=STARTED\n");
#endif
    return true;
}
bool validation_ap_netif_down(void)
{
#if defined(CONFIG_RW_LINK_DHCP) && defined(CONFIG_RW_LINK_AP)
    esp_err_t err = esp_netif_dhcps_stop(netif);
    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
        printf("RW_LINK_DHCP_SERVER=STOP_FAIL err=%d\n", err);
        return false;
    }
    printf("RW_LINK_DHCP_SERVER=STOPPED\n");
    taskENTER_CRITICAL(&lease_lock);
    memset(authorized, 0, sizeof(authorized));
    memset(leases, 0, sizeof(leases));
    taskEXIT_CRITICAL(&lease_lock);
#endif
    esp_netif_action_disconnected(netif, NULL, 0, NULL);
    return true;
}
bool validation_ap_lease_mac(uint32_t ip_addr, uint8_t mac[6])
{
#if defined(CONFIG_RW_LINK_DHCP) && defined(CONFIG_RW_LINK_AP)
    bool found = false;
    taskENTER_CRITICAL(&lease_lock);
    for (unsigned i = 0; i < 2; ++i) {
        if (leases[i].ip == ip_addr) {
            for (unsigned j = 0; j < 2; ++j) {
                if (authorized[j].present &&
                    memcmp(authorized[j].mac, leases[i].mac, 6) == 0) {
                    memcpy(mac, leases[i].mac, 6);
                    found = true;
                    break;
                }
            }
            break;
        }
    }
    taskEXIT_CRITICAL(&lease_lock);
    return found;
#else
    (void)ip_addr; (void)mac;
    return false;
#endif
}
#endif

/* The SDK wrapper leaves TX VIF unspecified, which is ambiguous after boot()
 * creates a STA VIF and AP mode creates a second VIF. Select the test role. */
static esp_err_t bench_transmit(void *handle, void *buffer, size_t len)
{
    (void)handle;
    if (mmwlan_tx_wait_until_ready(1000) != MMWLAN_SUCCESS) return ESP_FAIL;
    struct mmwlan_tx_metadata metadata = MMWLAN_TX_METADATA_INIT;
#ifdef CONFIG_RW_LINK_AP
    metadata.vif = MMWLAN_VIF_AP;
#else
    metadata.vif = MMWLAN_VIF_STA;
#endif
    struct mmpkt *pkt = mmwlan_alloc_mmpkt_for_tx(len, metadata.tid);
    if (!pkt) return ESP_ERR_NO_MEM;
    struct mmpktview *view = mmpkt_open(pkt);
    mmpkt_append_data(view, buffer, len);
    mmpkt_close(&view);
    return mmwlan_tx_pkt(pkt, &metadata) == MMWLAN_SUCCESS ? ESP_OK : ESP_FAIL;
}

static void bench_free_rx(void *handle, void *buffer)
{
    (void)handle;
    struct mmpktview *view = buffer;
    struct mmpkt *pkt = mmpkt_from_view(view);
    mmpkt_close(&view);
    mmpkt_release(pkt);
}

static esp_err_t bench_transmit_wrap(void *handle, void *buffer, size_t len, void *pbuf)
{
    (void)pbuf;
    return bench_transmit(handle, buffer, len);
}

static bool select_bench_channel(void)
{
    const struct mmwlan_s1g_channel_list *domain =
        mmwlan_lookup_regulatory_domain(get_regulatory_db(), CONFIG_HALOW_COUNTRY_CODE);
    if (!domain) return false;
    for (unsigned i = 0; i < domain->num_channels; ++i) {
        const struct mmwlan_s1g_channel *channel = &domain->channels[i];
        if (channel->s1g_chan_num != CONFIG_RW_LINK_CHANNEL ||
            channel->s1g_operating_class != CONFIG_RW_LINK_OPCLASS) continue;
        bench_channels = *domain;
        bench_channels.num_channels = 1;
        bench_channels.channels = channel;
        enum mmwlan_status status = mmwlan_set_channel_list(&bench_channels);
        printf("RW_LINK_CHANNEL freq_hz=%" PRIu32 " bw_mhz=%u status=%d\n",
               channel->centre_freq_hz, channel->bw_mhz, status);
        return status == MMWLAN_SUCCESS;
    }
    return false;
}

static void got_ip(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)id;
    ip_event_got_ip_t *event = data;
    if (event->esp_netif != netif) return;
    printf("RW_LINK_IP=" IPSTR "\n", IP2STR(&event->ip_info.ip));
#if defined(CONFIG_RW_LINK_DHCP) && defined(CONFIG_RW_LINK_STA)
    printf("RW_LINK_DHCP_LEASE role=STA ip=" IPSTR " gw=" IPSTR " mask=" IPSTR
           " acquire_ms=%" PRIu32 " t_ms=%" PRIu32 "\n", IP2STR(&event->ip_info.ip),
           IP2STR(&event->ip_info.gw), IP2STR(&event->ip_info.netmask),
           dhcp_wait_start_ms ? (uint32_t)(esp_timer_get_time() / 1000) - dhcp_wait_start_ms : 0,
           (uint32_t)(esp_timer_get_time() / 1000));
#endif
    xEventGroupSetBits(events, IP_BIT);
}

#if defined(CONFIG_RW_LINK_DHCP) && defined(CONFIG_RW_LINK_AP)
static void ap_lease(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)id;
    ip_event_ap_staipassigned_t *event = data;
    if (event->esp_netif != netif) return;
    taskENTER_CRITICAL(&lease_lock);
    unsigned slot = 2;
    for (unsigned i = 0; i < 2; ++i) {
        if (leases[i].ip == event->ip.addr ||
            memcmp(leases[i].mac, event->mac, 6) == 0) { slot = i; break; }
    }
    if (slot == 2) {
        for (unsigned i = 0; i < 2; ++i) {
            if (leases[i].ip == 0) { slot = i; break; }
        }
    }
    if (slot == 2) slot = 0;
    leases[slot].ip = event->ip.addr;
    memcpy(leases[slot].mac, event->mac, 6);
    taskEXIT_CRITICAL(&lease_lock);
    printf("RW_LINK_DHCP_LEASE role=AP ip=" IPSTR " mac=%02x:%02x:%02x:%02x:%02x:%02x t_ms=%" PRIu32 "\n",
           IP2STR(&event->ip), event->mac[0], event->mac[1], event->mac[2],
           event->mac[3], event->mac[4], event->mac[5],
           (uint32_t)(esp_timer_get_time() / 1000));
}
#endif

#ifndef CONFIG_RW_LINK_AP
static void scan_rx(const struct mmwlan_scan_result *result, void *arg)
{
    (void)arg;
    if (result->ssid_len != strlen(CONFIG_RW_LINK_SSID) ||
        memcmp(result->ssid, CONFIG_RW_LINK_SSID, result->ssid_len)) return;
    /* Bench sanity rule, not an SDK-defined invalid sentinel or calibrated SNR. */
    bool valid = result->rssi >= -127 && result->rssi <= -1 &&
                 result->noise_dbm >= -127 && result->noise_dbm <= -1;
    char snr[12] = "NA";
    if (valid) snprintf(snr, sizeof(snr), "%d", result->rssi - result->noise_dbm);
    printf("RW_LINK_SCAN_TARGET freq_hz=%" PRIu32
           " bw_mhz=%u op_bw_mhz=%u rssi_dbm=%d noise_dbm=%d"
           " scan_snr_db=%s scan_snr_status=%s\n",
           result->channel_freq_hz, result->bw_mhz, result->op_bw_mhz,
           result->rssi, result->noise_dbm, snr, valid ? "ok" : "out_of_range");
}

static void sta_event(const struct mmwlan_sta_event_cb_args *event, void *arg)
{
    (void)arg;
    printf("RW_LINK_STA_EVENT=%d\n", event->event);
}

static void sta_status(enum mmwlan_sta_state state)
{
    printf("RW_LINK_STA_STATE=%d\n", state);
    if (state == MMWLAN_STA_CONNECTED) {
        xEventGroupSetBits(events, LINK_BIT);
        status_led_set(RW_LED_CONNECTED);
    } else {
        xEventGroupClearBits(events, LINK_BIT | IP_BIT);
#ifdef CONFIG_RW_LINK_DHCP
        dhcp_wait_start_ms = (uint32_t)(esp_timer_get_time() / 1000);
#endif
        status_led_set(RW_LED_WAIT);
    }
}
#else
static void ap_sta_status(const struct mmwlan_ap_sta_status *status, void *arg)
{
    (void)arg;
    printf("RW_LINK_AP_STA_STATE=%d aid=%u\n", status->state, status->aid);
#ifdef CONFIG_RW_LINK_DHCP
    taskENTER_CRITICAL(&lease_lock);
    unsigned slot = 2;
    for (unsigned i = 0; i < 2; ++i) {
        if (authorized[i].present &&
            memcmp(authorized[i].mac, status->mac_addr, 6) == 0) { slot = i; break; }
    }
    if (status->state == MMWLAN_AP_STA_AUTHORIZED) {
        if (slot == 2) slot = !authorized[0].present ? 0 : 1;
        authorized[slot].present = true;
        memcpy(authorized[slot].mac, status->mac_addr, 6);
    } else if (slot != 2 && status->state == MMWLAN_AP_STA_UNKNOWN) {
        authorized[slot].present = false;
    }
    taskEXIT_CRITICAL(&lease_lock);
#endif
    status_led_set(status->state == MMWLAN_AP_STA_AUTHORIZED ? RW_LED_CONNECTED : RW_LED_WAIT);
}
#endif

static bool configure_ip(void)
{
    /* The pinned wrapper uses WIFI_STA_DEF for either role. Do not enable the
     * ESP32's internal 2.4 GHz Wi-Fi or rely on the official STA sample's fixed sleep. */
    netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!netif) return false;
    esp_netif_driver_ifconfig_t driver = {
        .handle = esp_netif_get_io_driver(netif), .transmit = bench_transmit,
        .transmit_wrap = bench_transmit_wrap, .driver_free_rx_buffer = bench_free_rx
    };
    if (esp_netif_set_driver_config(netif, &driver) != ESP_OK) return false;
    esp_err_t ret;
#if defined(CONFIG_RW_LINK_DHCP) && defined(CONFIG_RW_LINK_AP)
    ret = esp_netif_dhcps_stop(netif);
#elif defined(CONFIG_RW_LINK_DHCP) && defined(CONFIG_RW_LINK_STA)
    esp_netif_dhcp_status_t status;
    ret = esp_netif_dhcpc_get_status(netif, &status);
    if (ret != ESP_OK || status == ESP_NETIF_DHCP_STOPPED) return false;
    printf("RW_LINK_DHCP_CLIENT=ENABLED status=%d\n", status);
    return true;
#else
    ret = esp_netif_dhcpc_stop(netif);
#endif
    if (ret != ESP_OK && ret != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) return false;
    esp_netif_ip_info_t ip = {0};
#ifdef CONFIG_RW_LINK_AP
    esp_netif_str_to_ip4(AP_IP, &ip.ip);
#else
    esp_netif_str_to_ip4(STA_IP, &ip.ip);
#endif
    esp_netif_str_to_ip4(AP_IP, &ip.gw);
    esp_netif_str_to_ip4("255.255.255.0", &ip.netmask);
    return esp_netif_set_ip_info(netif, &ip) == ESP_OK;
}

static int make_socket(const char *local_ip, unsigned local_port)
{
    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (fd < 0) return -1;
    struct timeval timeout = { .tv_sec = 1 };
    struct sockaddr_in local = { .sin_family = AF_INET, .sin_port = htons(local_port) };
    inet_pton(AF_INET, local_ip, &local.sin_addr);
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0 ||
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) != 0 ||
        bind(fd, (struct sockaddr *)&local, sizeof(local)) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

#ifdef CONFIG_RW_LINK_AP
static bool echo_server(void)
{
    int fd = make_socket(AP_IP, PORT);
    if (fd < 0) return false;
    printf("RW_LINK_AP_READY ip=%s port=%u window_s=120\n", AP_IP, PORT);
    unsigned echoed = 0;
    int64_t end = esp_timer_get_time() + 120000000;
    struct in_addr expected;
    inet_pton(AF_INET, STA_IP, &expected);
    while (esp_timer_get_time() < end) {
        char buffer[128];
        struct sockaddr_in peer = {0};
        socklen_t peer_len = sizeof(peer);
        int len = recvfrom(fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&peer, &peer_len);
        if (len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) continue;
            printf("RW_LINK_SOCKET_ERROR=%d\n", errno);
            break;
        }
        if (peer.sin_addr.s_addr != expected.s_addr || len < 8 || memcmp(buffer, "RWLINK1 ", 8)) continue;
        if (sendto(fd, buffer, len, 0, (struct sockaddr *)&peer, peer_len) == len) {
            ++echoed;
            status_led_activity();
        }
    }
    close(fd);
    printf("RW_LINK_AP_ECHOED=%u; end-to-end result is reported by STA\n", echoed);
    return echoed >= PROBES;
}
#else
static bool udp_probes(void)
{
    int fd = make_socket(STA_IP, 0);
    if (fd < 0) return false;
    struct sockaddr_in ap = { .sin_family = AF_INET, .sin_port = htons(PORT) };
    inet_pton(AF_INET, AP_IP, &ap.sin_addr);
    if (connect(fd, (struct sockaddr *)&ap, sizeof(ap)) != 0) { close(fd); return false; }
    uint32_t nonce = esp_random();
    unsigned received = 0, sent = 0;
    int64_t total_us = 0, max_us = 0;
    for (unsigned seq = 0; seq < PROBES; ++seq) {
        char tx[64], rx[128];
        int len = snprintf(tx, sizeof(tx), "RWLINK1 %08" PRIx32 " %u", nonce, seq);
        int64_t begin = esp_timer_get_time();
        if (send(fd, tx, len, 0) != len) {
            printf("RW_LINK_SEND_FAIL seq=%u errno=%d\n", seq, errno);
            continue;
        }
        ++sent;
        bool match = false;
        /* Ignore delayed/duplicate replies; only this nonce + sequence counts. */
        while (esp_timer_get_time() - begin < 1000000) {
            int n = recv(fd, rx, sizeof(rx), 0);
            if (n < 0) break;
            if (n == len && memcmp(rx, tx, len) == 0) { match = true; break; }
        }
        int64_t elapsed = esp_timer_get_time() - begin;
        if (match) {
            ++received;
            status_led_activity();
            total_us += elapsed;
            if (elapsed > max_us) max_us = elapsed;
            printf("RW_LINK_ECHO seq=%u rtt_us=%" PRId64 "\n", seq, elapsed);
        } else printf("RW_LINK_TIMEOUT seq=%u\n", seq);
        vTaskDelay(pdMS_TO_TICKS(250));
    }
    close(fd);
    printf("RW_LINK_UDP sent=%u received=%u planned=%u rtt_mean_us=%" PRId64 " rtt_max_us=%" PRId64 "\n",
           sent, received, PROBES, received ? total_us / received : 0, max_us);
    return received == PROBES;
}
#endif

bool run_radio_test(void)
{
    size_t ssid_len = strlen(CONFIG_RW_LINK_SSID), psk_len = strlen(CONFIG_RW_LINK_PSK);
    if (!ssid_len || ssid_len > MMWLAN_SSID_MAXLEN || psk_len < 8 || psk_len > 63 ||
        strcmp(CONFIG_HALOW_COUNTRY_CODE, "US") != 0 ||
        CONFIG_RW_LINK_CHANNEL == 0 || CONFIG_RW_LINK_OPCLASS == 0) {
        printf("RW_LINK_CONFIG=FAIL; set SSID, local PSK, board country, channel and op-class before RF boot\n");
        return false;
    }
    /* Do not silently erase existing NVS on an initialization error. */
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_netif_init());
    events = xEventGroupCreate();
    if (!events) return false;
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, got_ip, NULL));
#if defined(CONFIG_RW_LINK_DHCP) && defined(CONFIG_RW_LINK_AP)
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, ap_lease, NULL));
    validation_dhcp_wrap_begin();
    esp_err_t init_result = mmhalow_init(NULL);
    validation_dhcp_wrap_end();
    ESP_ERROR_CHECK(init_result);
#else
    ESP_ERROR_CHECK(mmhalow_init(NULL));
#endif
    bool ok = false;
    bool shutdown_ok = true;
#ifdef CONFIG_RW_LINK_CONTINUOUS
    radio_shutdown_ok = false;
#endif
#ifdef CONFIG_RW_LINK_AP
    bool ap_started = false;
#else
    bool sta_started = false;
#endif
    struct mmwlan_version version = {0};
    if (mmwlan_get_version(&version) != MMWLAN_SUCCESS || !version.morse_chip_id) goto done;
    /* mmhalow_init() boots a placeholder interface. Channel-list changes are
     * only legal with all interfaces stopped. Keep its netif, then boot again
     * with the bench channel selected. No AP/STA has been enabled yet. */
    if (mmwlan_shutdown() != MMWLAN_SUCCESS) goto done;
    if (!select_bench_channel()) goto done;
    struct mmwlan_boot_args boot = MMWLAN_BOOT_ARGS_INIT;
    if (mmwlan_boot(&boot) != MMWLAN_SUCCESS) goto done;
    if (mmwlan_set_power_save_mode(MMWLAN_PS_DISABLED) != MMWLAN_SUCCESS) goto done;
    enum mmwlan_status power_status = mmwlan_override_max_tx_power(CONFIG_RW_LINK_TX_POWER_DBM);
    if (power_status != MMWLAN_SUCCESS) {
        printf("RW_LINK_POWER_OVERRIDE_FAIL=%d\n", power_status);
        goto done;
    }
    if (!configure_ip()) goto done;
    printf("RW_LINK_RADIO_CONFIG country=%s channel=%d opclass=%d max_tx_dbm=%d psk=REDACTED\n",
           CONFIG_HALOW_COUNTRY_CODE, CONFIG_RW_LINK_CHANNEL, CONFIG_RW_LINK_OPCLASS, CONFIG_RW_LINK_TX_POWER_DBM);
#ifdef CONFIG_RW_LINK_AP
    struct mmwlan_ap_args ap = MMWLAN_AP_ARGS_INIT;
    memcpy(ap.ssid, CONFIG_RW_LINK_SSID, ssid_len);
    ap.ssid_len = ssid_len;
    memcpy(ap.passphrase, CONFIG_RW_LINK_PSK, psk_len);
    ap.passphrase_len = psk_len;
    ap.security_type = MMWLAN_SAE;
    ap.pmf_mode = MMWLAN_PMF_REQUIRED;
    ap.s1g_chan_num = CONFIG_RW_LINK_CHANNEL;
    ap.op_class = CONFIG_RW_LINK_OPCLASS;
#ifdef CONFIG_RW_LINK_CONTINUOUS
    ap.max_stas = 2;
#else
    ap.max_stas = 1;
#endif
    ap.sta_status_cb = ap_sta_status;
    enum mmwlan_status ap_status = mmwlan_ap_enable(&ap);
    if (ap_status != MMWLAN_SUCCESS) {
        printf("RW_LINK_AP_START_FAIL=%d\n", ap_status);
        goto done;
    }
    ap_started = true;
    uint8_t ap_mac[6];
    if (mmwlan_get_vif_mac_addr(MMWLAN_VIF_AP, ap_mac) != MMWLAN_SUCCESS ||
        esp_netif_set_mac(netif, ap_mac) != ESP_OK) goto done;
    printf("RW_LINK_AP_MAC=%02x:%02x:%02x:%02x:%02x:%02x\n",
           ap_mac[0], ap_mac[1], ap_mac[2], ap_mac[3], ap_mac[4], ap_mac[5]);
#ifdef CONFIG_RW_LINK_CONTINUOUS
    if (!validation_ap_netif_up()) goto done;
#else
    esp_netif_action_connected(netif, NULL, 0, NULL);
#endif
#ifdef CONFIG_RW_LINK_CONTINUOUS
#ifdef CONFIG_RW_LINK_THROUGHPUT
    ok = run_throughput();
#else
    ok = run_validation_ap(&ap);
#endif
#else
    ok = echo_server();
#endif
#else
    mmhalow_wifi_config_t sta = { .sta = MMWLAN_STA_ARGS_INIT };
    memcpy(sta.sta.ssid, CONFIG_RW_LINK_SSID, ssid_len);
    sta.sta.ssid_len = ssid_len;
    memcpy(sta.sta.passphrase, CONFIG_RW_LINK_PSK, psk_len);
    sta.sta.passphrase_len = psk_len;
    sta.sta.security_type = MMWLAN_SAE;
    sta.sta.scan_rx_cb = scan_rx;
    sta.sta.sta_evt_cb = sta_event;
    ESP_ERROR_CHECK(mmhalow_set_config(WIFI_IF_STA, &sta));
#ifdef CONFIG_RW_LINK_DHCP
    dhcp_wait_start_ms = (uint32_t)(esp_timer_get_time() / 1000);
#endif
    enum mmwlan_status sta_start = mmhalow_connect(sta_status);
    printf("RW_LINK_STA_START=%d\n", sta_start);
    if (sta_start != MMWLAN_SUCCESS) goto done;
    sta_started = true;
#ifdef CONFIG_RW_LINK_CONTINUOUS
#ifdef CONFIG_RW_LINK_THROUGHPUT
    ok = run_throughput();
#else
    ok = run_validation_sta();
#endif
#else
    EventBits_t bits = xEventGroupWaitBits(events, LINK_BIT | IP_BIT, pdFALSE, pdTRUE, pdMS_TO_TICKS(30000));
    if ((bits & (LINK_BIT | IP_BIT)) == (LINK_BIT | IP_BIT)) ok = udp_probes();
    else printf("RW_LINK_CONNECT_TIMEOUT bits=%u\n", (unsigned)bits);
#endif
#endif
done:
#ifdef CONFIG_RW_LINK_AP
    if (ap_started) {
#ifdef CONFIG_RW_LINK_CONTINUOUS
        if (!validation_ap_netif_down()) { ok = false; shutdown_ok = false; }
#endif
        if (mmwlan_ap_disable() != MMWLAN_SUCCESS) { ok = false; shutdown_ok = false; }
    }
#else
    if (sta_started && mmhalow_disconnect() != MMWLAN_SUCCESS) {
        ok = false;
        shutdown_ok = false;
    }
#endif
    /* Keep event storage alive until reboot: the driver may have queued callbacks. */
    if (mmhalow_deinit() != MMWLAN_SUCCESS) { ok = false; shutdown_ok = false; }
    if (!shutdown_ok) ok = false;
#ifdef CONFIG_RW_LINK_CONTINUOUS
    radio_shutdown_ok = shutdown_ok;
#endif
    printf("RW_LINK_RADIO_SHUTDOWN\n");
    return ok;
}
