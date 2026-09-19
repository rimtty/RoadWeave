#include "sdkconfig.h"
#ifdef CONFIG_RW_LINK_CONTINUOUS

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include "arpa/inet.h"
#include "esp_heap_caps.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "link_test.h"

#define VALIDATION_PORT 3333
#define VALIDATION_MAGIC 0x52574c32U /* RWL2 */
#define RTT_TIMEOUT_US 1000000LL
#define SAMPLE_PERIOD_US 10000000LL
#ifndef CONFIG_RW_LINK_STA_ID
#define CONFIG_RW_LINK_STA_ID 0
#endif
#ifndef CONFIG_RW_LINK_MAX_PROBES
#define CONFIG_RW_LINK_MAX_PROBES 0
#endif
#ifndef CONFIG_RW_LINK_INTERVAL_MS
#define CONFIG_RW_LINK_INTERVAL_MS 250
#endif

enum control_command { CMD_NONE, CMD_RESTART, CMD_AP_OFF_10S, CMD_STOP };
static atomic_int pending_command;
static TaskHandle_t reader_task;

struct packet_header {
    uint32_t magic;
    uint32_t nonce;
    uint32_t seq;
    uint32_t id;
};

struct rc_counters {
    bool available;
    uint64_t sent;
    uint64_t success;
};

struct run_stats {
    uint32_t planned;
    uint32_t sent;
    uint32_t received;
    uint32_t send_fail;
    uint32_t skipped;
    uint32_t duplicates;
    uint32_t late;
    uint32_t invalid;
    uint32_t reconnects;
    uint32_t last_matched_seq;
    uint64_t offered_bytes;
    uint64_t useful_bytes;
    uint32_t rtt_max_us;
    uint32_t rtt_bins[1001]; /* one millisecond; timeout is one second */
    int64_t outage_start_us;
    const char *outage_reason;
    bool ever_received;
};

static void command_reader(void *arg)
{
    (void)arg;
    char line[64];
    for (;;) {
        if (!fgets(line, sizeof(line), stdin)) {
            clearerr(stdin);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        if (strcmp(line, "RW_LINK_CMD RESTART\n") == 0 ||
            strcmp(line, "RW_LINK_CMD RESTART\r\n") == 0) {
            atomic_store(&pending_command, CMD_RESTART);
        } else if (strcmp(line, "RW_LINK_CMD AP_OFF_10S\n") == 0 ||
                   strcmp(line, "RW_LINK_CMD AP_OFF_10S\r\n") == 0) {
            atomic_store(&pending_command, CMD_AP_OFF_10S);
        } else if (strcmp(line, "RW_LINK_CMD STOP\n") == 0 ||
                   strcmp(line, "RW_LINK_CMD STOP\r\n") == 0) {
            atomic_store(&pending_command, CMD_STOP);
        } else {
            printf("RW_LINK_CMD_REJECT reason=unknown\n");
        }
    }
}

static bool start_reader(void)
{
    atomic_store(&pending_command, CMD_NONE);
    return xTaskCreate(command_reader, "rw_link_cmd", 4096, NULL, 4, &reader_task) == pdPASS;
}

static void stop_reader(void)
{
    if (reader_task) {
        vTaskDelete(reader_task);
        reader_task = NULL;
    }
}

static enum control_command take_command(void)
{
    enum control_command command = atomic_exchange(&pending_command, CMD_NONE);
    if (command == CMD_RESTART) {
        printf("RW_LINK_CMD_ACK command=RESTART\n");
        fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(200));
        esp_restart();
    }
    return command;
}

static int open_udp(const char *ip, uint16_t port)
{
    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (fd < 0) return -1;
    struct timeval timeout = { .tv_sec = 0, .tv_usec = 200000 };
    struct sockaddr_in local = { .sin_family = AF_INET, .sin_port = htons(port) };
    if (inet_pton(AF_INET, ip, &local.sin_addr) != 1 ||
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0 ||
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) != 0 ||
        bind(fd, (struct sockaddr *)&local, sizeof(local)) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static struct rc_counters read_rc(void)
{
    struct rc_counters out = {0};
    struct mmwlan_rc_stats *rc = mmwlan_get_rc_stats();
    if (!rc) return out;
    out.available = true;
    for (uint32_t i = 0; i < rc->n_entries; ++i) {
        out.sent += rc->total_sent[i];
        out.success += rc->total_success[i];
    }
    mmwlan_free_rc_stats(rc);
    return out;
}

static uint32_t percentile_us(const struct run_stats *s, unsigned percent)
{
    if (!s->received) return 0;
    uint32_t target = (s->received * percent + 99) / 100;
    uint32_t count = 0;
    for (uint32_t i = 0; i < 1001; ++i) {
        count += s->rtt_bins[i];
        if (count >= target) return i * 1000;
    }
    return s->rtt_max_us;
}

static void print_sta_stats(const char *kind, const struct run_stats *s,
                            int64_t begin_us, struct rc_counters rc_start)
{
    int64_t elapsed_us = esp_timer_get_time() - begin_us;
    if (elapsed_us <= 0) elapsed_us = 1;
    struct rc_counters rc_end = read_rc();
    int32_t rssi = mmwlan_get_rssi();
    printf("RW_LINK_%s role=STA id=%d elapsed_ms=%" PRId64
           " planned=%" PRIu32 " sent=%" PRIu32 " received=%" PRIu32
           " lost=%" PRIu32 " send_fail=%" PRIu32 " skipped=%" PRIu32
           " duplicates=%" PRIu32 " late=%" PRIu32 " invalid=%" PRIu32
           " reconnects=%" PRIu32
           " rtt_p50_us=%" PRIu32 " rtt_p95_us=%" PRIu32 " rtt_p99_us=%" PRIu32
           " rtt_max_us=%" PRIu32 " offered_bps=%" PRIu64 " useful_bps=%" PRIu64
           " heap_free=%u heap_min=%u rssi_dbm=%" PRId32 " snr_db=NA",
           kind, CONFIG_RW_LINK_STA_ID, elapsed_us / 1000,
           s->planned, s->sent, s->received, s->sent - s->received,
           s->send_fail, s->skipped, s->duplicates, s->late, s->invalid, s->reconnects,
           percentile_us(s, 50), percentile_us(s, 95), percentile_us(s, 99),
           s->rtt_max_us, s->offered_bytes * 8000000ULL / elapsed_us,
           s->useful_bytes * 8000000ULL / elapsed_us,
           (unsigned)esp_get_free_heap_size(),
           (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT), rssi);
    if (rc_start.available && rc_end.available) {
        printf(" rc_sent_start=%" PRIu64 " rc_sent_end=%" PRIu64
               " rc_success_start=%" PRIu64 " rc_success_end=%" PRIu64,
               rc_start.sent, rc_end.sent, rc_start.success, rc_end.success);
    } else printf(" rc_sent_start=NA rc_sent_end=NA rc_success_start=NA rc_success_end=NA");
    printf("\n");
}

static void record_outage(struct run_stats *s, const char *reason)
{
    if (s->ever_received && !s->outage_start_us) {
        s->outage_start_us = esp_timer_get_time();
        s->outage_reason = reason;
        printf("RW_LINK_OUTAGE reason=%s elapsed_ms=%" PRId64 "\n",
               reason, s->outage_start_us / 1000);
    }
}

static void record_match(struct run_stats *s, uint32_t seq, uint32_t rtt_us)
{
    s->received++;
    s->ever_received = true;
    s->last_matched_seq = seq;
    if (rtt_us > s->rtt_max_us) s->rtt_max_us = rtt_us;
    unsigned bin = rtt_us / 1000;
    if (bin > 1000) bin = 1000;
    s->rtt_bins[bin]++;
    printf("RW_LINK_ECHO_MATCH seq=%" PRIu32 " rtt_us=%" PRIu32 "\n", seq, rtt_us);
    if (s->outage_start_us) {
        s->reconnects++;
        printf("RW_LINK_RECOVERY reason=%s downtime_ms=%" PRId64
               " seq=%" PRIu32 "\n", s->outage_reason,
               (esp_timer_get_time() - s->outage_start_us) / 1000, seq);
        s->outage_start_us = 0;
    }
    status_led_activity();
}

bool run_validation_sta(void)
{
    const char *local_ip = CONFIG_RW_LINK_STA_ID == 1 ? "192.168.50.2" : "192.168.50.3";
    int fd = open_udp(local_ip, 0);
    if (fd < 0) {
        printf("RW_LINK_SOCKET_ERROR phase=sta_open errno=%d\n", errno);
        return false;
    }
    struct sockaddr_in ap = { .sin_family = AF_INET, .sin_port = htons(VALIDATION_PORT) };
    inet_pton(AF_INET, "192.168.50.1", &ap.sin_addr);
    if (connect(fd, (struct sockaddr *)&ap, sizeof(ap)) != 0) {
        close(fd);
        return false;
    }
    if (!start_reader()) { close(fd); return false; }
    uint32_t nonce = esp_random();
    static struct run_stats stats;
    memset(&stats, 0, sizeof(stats));
    struct rc_counters rc_start = read_rc();
    int64_t begin = esp_timer_get_time();
    int64_t end = begin + (int64_t)CONFIG_RW_LINK_RUN_SECONDS * 1000000;
    int64_t next = begin;
    int64_t sample = begin + SAMPLE_PERIOD_US;
    printf("RW_LINK_RUN_START role=STA id=%d ip=%s run_id=%08" PRIx32
           " duration_s=%d max_probes=%d payload_bytes=%d interval_ms=%d\n",
           CONFIG_RW_LINK_STA_ID, local_ip, nonce, CONFIG_RW_LINK_RUN_SECONDS,
           CONFIG_RW_LINK_MAX_PROBES, CONFIG_RW_LINK_PAYLOAD_BYTES, CONFIG_RW_LINK_INTERVAL_MS);
    while (esp_timer_get_time() < end &&
           (!CONFIG_RW_LINK_MAX_PROBES || stats.planned < CONFIG_RW_LINK_MAX_PROBES)) {
        enum control_command command = take_command();
        if (command == CMD_STOP) { printf("RW_LINK_CMD_ACK command=STOP\n"); break; }
        if (command == CMD_AP_OFF_10S) printf("RW_LINK_CMD_REJECT reason=sta_role\n");
        int64_t now = esp_timer_get_time();
        if (now < next) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        next += (int64_t)CONFIG_RW_LINK_INTERVAL_MS * 1000;
        stats.planned++;
        if (!validation_link_ready()) {
            stats.skipped++;
            record_outage(&stats, "link_down");
        } else {
            uint8_t tx[CONFIG_RW_LINK_PAYLOAD_BYTES];
            uint8_t rx[CONFIG_RW_LINK_PAYLOAD_BYTES];
            struct packet_header header = {
                htonl(VALIDATION_MAGIC), htonl(nonce),
                htonl(stats.planned), htonl(CONFIG_RW_LINK_STA_ID)
            };
            memcpy(tx, &header, sizeof(header));
            for (unsigned i = sizeof(header); i < sizeof(tx); ++i) tx[i] = (uint8_t)(i + stats.planned);
            int64_t sent_at = esp_timer_get_time();
            if (send(fd, tx, sizeof(tx), 0) != (ssize_t)sizeof(tx)) {
                stats.send_fail++;
                printf("RW_LINK_SEND_FAIL seq=%" PRIu32 " errno=%d\n", stats.planned, errno);
                record_outage(&stats, "send_fail");
            } else {
                stats.sent++;
                stats.offered_bytes += sizeof(tx);
                bool matched = false;
                while (esp_timer_get_time() - sent_at < RTT_TIMEOUT_US && esp_timer_get_time() < end) {
                    int n = recv(fd, rx, sizeof(rx), 0);
                    if (n < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) continue;
                        break;
                    }
                    if (n < (int)sizeof(header)) { stats.invalid++; continue; }
                    struct packet_header got;
                    memcpy(&got, rx, sizeof(got));
                    if (got.magic != htonl(VALIDATION_MAGIC) || got.nonce != htonl(nonce) ||
                        got.id != htonl(CONFIG_RW_LINK_STA_ID)) { stats.invalid++; continue; }
                    uint32_t rx_seq = ntohl(got.seq);
                    if (rx_seq == stats.planned && n == (int)sizeof(tx) && memcmp(rx, tx, n) == 0) {
                        uint32_t rtt_us = (uint32_t)(esp_timer_get_time() - sent_at);
                        record_match(&stats, rx_seq, rtt_us);
                        stats.useful_bytes += n;
                        matched = true;
                        break;
                    }
                    if (rx_seq == stats.last_matched_seq && stats.last_matched_seq != 0) {
                        stats.duplicates++;
                        printf("RW_LINK_ECHO_DUPLICATE seq=%" PRIu32 "\n", rx_seq);
                    } else stats.late++;
                }
                if (!matched) {
                    printf("RW_LINK_TIMEOUT seq=%" PRIu32 "\n", stats.planned);
                    record_outage(&stats, "probe_timeout");
                }
            }
        }
        now = esp_timer_get_time();
        while (next <= now && next < end &&
               (!CONFIG_RW_LINK_MAX_PROBES || stats.planned < CONFIG_RW_LINK_MAX_PROBES)) {
            stats.planned++;
            stats.skipped++;
            next += (int64_t)CONFIG_RW_LINK_INTERVAL_MS * 1000;
        }
        if (now >= sample) {
            print_sta_stats("SAMPLE", &stats, begin, rc_start);
            sample = now + SAMPLE_PERIOD_US;
        }
    }
    print_sta_stats("SUMMARY", &stats, begin, rc_start);
    printf("RW_LINK_RUN_END role=STA run_id=%08" PRIx32 "\n", nonce);
    stop_reader();
    close(fd);
    /* Quality gates (loss/latency/recovery) are evaluated from the summary. */
    return stats.received > 0;
}

struct ap_stats {
    uint32_t echoed[3];
    uint32_t invalid;
    uint32_t send_fail;
    uint64_t echoed_bytes;
};

static void print_ap_stats(const char *kind, const struct ap_stats *s, int64_t begin,
                           struct rc_counters rc_start)
{
    int64_t elapsed_us = esp_timer_get_time() - begin;
    if (elapsed_us <= 0) elapsed_us = 1;
    struct rc_counters rc_end = read_rc();
    printf("RW_LINK_%s role=AP elapsed_ms=%" PRId64
           " echo_id1=%" PRIu32 " echo_id2=%" PRIu32
           " invalid=%" PRIu32 " send_fail=%" PRIu32
           " useful_bps=%" PRIu64 " heap_free=%u heap_min=%u snr_db=NA",
           kind, elapsed_us / 1000, s->echoed[1], s->echoed[2],
           s->invalid, s->send_fail, s->echoed_bytes * 8000000ULL / elapsed_us,
           (unsigned)esp_get_free_heap_size(),
           (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT));
    if (rc_start.available && rc_end.available) {
        printf(" rc_sent_start=%" PRIu64 " rc_sent_end=%" PRIu64
               " rc_success_start=%" PRIu64 " rc_success_end=%" PRIu64,
               rc_start.sent, rc_end.sent, rc_start.success, rc_end.success);
    } else printf(" rc_sent_start=NA rc_sent_end=NA rc_success_start=NA rc_success_end=NA");
    printf("\n");
}

bool run_validation_ap(const struct mmwlan_ap_args *ap)
{
    int fd = open_udp("192.168.50.1", VALIDATION_PORT);
    if (fd < 0) {
        printf("RW_LINK_SOCKET_ERROR phase=ap_open errno=%d\n", errno);
        return false;
    }
    if (!start_reader()) { close(fd); return false; }
    struct ap_stats stats = {0};
    struct rc_counters rc_start = read_rc();
    int64_t begin = esp_timer_get_time();
    int64_t end = begin + (int64_t)CONFIG_RW_LINK_RUN_SECONDS * 1000000;
    int64_t sample = begin + SAMPLE_PERIOD_US;
    uint32_t run_id = esp_random();
    bool execution_ok = true;
    printf("RW_LINK_RUN_START role=AP id=0 ip=192.168.50.1 run_id=%08" PRIx32
           " duration_s=%d payload_bytes=%d interval_ms=NA max_stas=2\n",
           run_id, CONFIG_RW_LINK_RUN_SECONDS, CONFIG_RW_LINK_PAYLOAD_BYTES);
    printf("RW_LINK_AP_READY ip=192.168.50.1 port=%d window_s=%d\n",
           VALIDATION_PORT, CONFIG_RW_LINK_RUN_SECONDS);
    while (esp_timer_get_time() < end) {
        enum control_command command = take_command();
        if (command == CMD_STOP) { printf("RW_LINK_CMD_ACK command=STOP\n"); break; }
        if (command == CMD_AP_OFF_10S) {
            if (end - esp_timer_get_time() < 12000000LL) {
                printf("RW_LINK_CMD_REJECT reason=insufficient_time command=AP_OFF_10S\n");
                continue;
            }
            printf("RW_LINK_CMD_ACK command=AP_OFF_10S\n");
            enum mmwlan_status disabled = mmwlan_ap_disable();
            printf("RW_LINK_AP_SERVICE state=off status=%d\n", disabled);
            if (disabled != MMWLAN_SUCCESS) { execution_ok = false; break; }
            vTaskDelay(pdMS_TO_TICKS(10000));
            enum mmwlan_status enabled = mmwlan_ap_enable(ap);
            printf("RW_LINK_AP_SERVICE state=on status=%d\n", enabled);
            if (enabled != MMWLAN_SUCCESS) { execution_ok = false; break; }
            validation_ap_netif_up();
            printf("RW_LINK_AP_READY ip=192.168.50.1 port=%d window_s=%d\n",
                   VALIDATION_PORT, (int)((end - esp_timer_get_time()) / 1000000));
        }
        uint8_t buffer[1024];
        struct sockaddr_in peer = {0};
        socklen_t peer_len = sizeof(peer);
        int n = recvfrom(fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&peer, &peer_len);
        if (n >= (int)sizeof(struct packet_header)) {
            struct packet_header header;
            memcpy(&header, buffer, sizeof(header));
            uint32_t id = ntohl(header.id);
            uint32_t source = ntohl(peer.sin_addr.s_addr);
            if (header.magic == htonl(VALIDATION_MAGIC) && id >= 1 && id <= 2 &&
                source == 0xc0a83201U + id) {
                if (sendto(fd, buffer, n, 0, (struct sockaddr *)&peer, peer_len) == n) {
                    stats.echoed[id]++;
                    stats.echoed_bytes += n;
                    status_led_activity();
                } else stats.send_fail++;
            } else stats.invalid++;
        } else if (n >= 0) stats.invalid++;
        else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
            printf("RW_LINK_SOCKET_ERROR phase=ap_recv errno=%d\n", errno);
            execution_ok = false;
            break;
        }
        int64_t now = esp_timer_get_time();
        if (now >= sample) {
            print_ap_stats("SAMPLE", &stats, begin, rc_start);
            sample = now + SAMPLE_PERIOD_US;
        }
    }
    print_ap_stats("SUMMARY", &stats, begin, rc_start);
    printf("RW_LINK_RUN_END role=AP run_id=%08" PRIx32 "\n", run_id);
    stop_reader();
    close(fd);
    return execution_ok && stats.echoed[1] + stats.echoed[2] > 0;
}

#endif
