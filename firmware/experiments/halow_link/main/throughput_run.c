#include "sdkconfig.h"
#ifdef CONFIG_RW_LINK_THROUGHPUT

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include "arpa/inet.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "link_test.h"

/* The wire format is deliberately independent of mmiperf's aggregate report:
 * only exact, unique, verified application bytes count as receiver goodput. */
#define TPUT_PORT 3334
#define TPUT_MAGIC 0x52575450U /* RWTP */
#define TPUT_VERSION 1
#define TPUT_MIN_PACKET 64
#define TPUT_MAX_PACKET 1200
#define TPUT_MAX_SEQ 262144U
#define TPUT_BITMAP_BYTES (TPUT_MAX_SEQ / 8)
#define TPUT_DRAIN_US 3000000LL
#define TPUT_CTRL_TIMEOUT_US 3000000LL
#define TPUT_RX_START_TIMEOUT_US 20000000LL
#define TPUT_SAMPLE_US 1000000LL
#define TPUT_SOCKET_TIMEOUT_US 100000
#define TPUT_MAX_STAGE_S 120
#define TPUT_AP_IP "192.168.50.1"
#ifndef CONFIG_RW_LINK_STA_ID
#define CONFIG_RW_LINK_STA_ID 0
#endif

enum packet_type {
    PKT_START = 1, PKT_DATA = 2, PKT_END = 3,
    PKT_ACK_START = 4, PKT_ACK_END = 5
};
enum command_type { COMMAND_ARM, COMMAND_SEND, COMMAND_DRAIN, COMMAND_ABORT, COMMAND_STOP };
enum stage_result { STAGE_OK, STAGE_FAILED, STAGE_STOP };

/* Natural layout is exactly 16 bytes; all multi-byte fields are network order. */
struct tput_header {
    uint32_t magic;
    uint32_t stage;
    uint32_t seq;
    uint16_t packet_bytes;
    uint8_t version;
    uint8_t type;
};
_Static_assert(sizeof(struct tput_header) == 16, "throughput wire header size");

struct tput_command {
    enum command_type type;
    uint32_t stage;
    uint32_t duration_s;
    uint32_t rate_kbps;
    uint32_t sent;
    uint16_t packet_bytes;
    char peer[16];
};

struct rx_stats {
    uint32_t active_unique;
    uint32_t late_unique;
    uint32_t drain_unique;
    uint32_t duplicate;
    uint32_t invalid;
    uint32_t wrong_stage;
    uint32_t wrong_peer;
    uint32_t overflow;
    uint32_t out_of_order;
    uint32_t high_seq;
    uint32_t end_sent;
    uint32_t drain_sent;
    int64_t first_active_us;
    int64_t last_active_us;
    int64_t start_us;
    int64_t end_us;
    int64_t drain_start_us;
    int64_t drain_end_us;
    bool start_seen;
    bool end_seen;
    bool drain_command;
    bool aborted;
};

struct tx_stats {
    uint32_t attempts;
    uint32_t sent;
    uint32_t send_fail;
    int64_t start_us;
    int64_t end_us;
    bool start_acked;
    bool end_acked;
    bool aborted;
};

static QueueHandle_t command_queue;
static TaskHandle_t command_task;

static void flush_control_log(void)
{
    /* A final USB Serial/JTAG transfer of exactly 64 bytes needs a zero-byte
     * packet to release it on the host. IDF's VFS fsync explicitly performs
     * that extra FIFO flush; fflush alone only empties newlib's buffer. */
    (void)fflush(stdout);
    (void)fsync(fileno(stdout));
}

static void print_reject(const char *reason)
{
    printf("RW_TPUT_CMD_REJECT reason=%s\n", reason);
    flush_control_log();
}

static bool parse_command(const char *line, struct tput_command *out)
{
    unsigned stage, duration, rate, packet, sent;
    char peer[16];
    int n = 0;
    memset(out, 0, sizeof(*out));
    if (strcmp(line, "RW_TPUT_CMD STOP") == 0 ||
        strcmp(line, "RW_LINK_CMD STOP") == 0) {
        out->type = COMMAND_STOP;
        return true;
    }
    if (sscanf(line, "RW_TPUT_CMD ABORT stage=%u%n", &stage, &n) == 1 &&
        n && line[n] == '\0') {
        out->type = COMMAND_ABORT;
        out->stage = stage;
        return true;
    }
    n = 0;
    if (sscanf(line, "RW_TPUT_CMD DRAIN stage=%u sent=%u%n", &stage, &sent, &n) == 2 &&
        n && line[n] == '\0') {
        out->type = COMMAND_DRAIN;
        out->stage = stage;
        out->sent = sent;
        return true;
    }
    n = 0;
    if (sscanf(line, "RW_TPUT_CMD ARM stage=%u peer=%15s duration_s=%u packet_bytes=%u%n",
               &stage, peer, &duration, &packet, &n) == 4 &&
        n && line[n] == '\0') {
        out->type = COMMAND_ARM;
    } else {
        n = 0;
        if (sscanf(line, "RW_TPUT_CMD SEND stage=%u peer=%15s duration_s=%u rate_kbps=%u packet_bytes=%u%n",
                   &stage, peer, &duration, &rate, &packet, &n) != 5 ||
            !n || line[n] != '\0') return false;
        out->type = COMMAND_SEND;
        out->rate_kbps = rate;
    }
    if (!stage || duration < 5 || duration > TPUT_MAX_STAGE_S ||
        packet < TPUT_MIN_PACKET || packet > TPUT_MAX_PACKET) return false;
    struct in_addr addr;
    if (inet_pton(AF_INET, peer, &addr) != 1) return false;
    out->stage = stage;
    out->duration_s = duration;
    out->packet_bytes = packet;
    strcpy(out->peer, peer);
    return true;
}

static void read_commands(void *arg)
{
    (void)arg;
    char line[160];
    size_t len = 0;
    bool overflow = false;
    for (;;) {
        /* USB Serial/JTAG can return a short read before the newline. Keep
         * fragments until a complete command arrives; length is checked
         * across all fragments, not per stdio read. */
        int ch = fgetc(stdin);
        if (ch == EOF) {
            clearerr(stdin);
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        if (ch != '\n') {
            if (!overflow) {
                if (len < sizeof(line) - 1) line[len++] = (char)ch;
                else overflow = true;
            }
            continue;
        }
        if (overflow) {
            print_reject("too_long");
            len = 0;
            overflow = false;
            continue;
        }
        while (len && line[len - 1] == '\r') --len;
        line[len] = '\0';
        len = 0;
        struct tput_command command;
        if (!parse_command(line, &command)) {
            print_reject("syntax_or_range");
            continue;
        }
        if (xQueueSend(command_queue, &command, 0) != pdPASS) print_reject("queue_full");
    }
}

static bool local_ip(char *ip, size_t size)
{
#ifdef CONFIG_RW_LINK_AP
    return snprintf(ip, size, "%s", TPUT_AP_IP) > 0;
#else
    return validation_sta_ip(ip, size);
#endif
}

static bool allowed_peer(const struct tput_command *command)
{
    struct in_addr address;
    if (inet_pton(AF_INET, command->peer, &address) != 1) return false;
#ifdef CONFIG_RW_LINK_AP
#ifdef CONFIG_RW_LINK_DHCP
    uint8_t mac[6];
    return validation_ap_lease_mac(address.s_addr, mac);
#else
    return strcmp(command->peer, "192.168.50.2") == 0 ||
           strcmp(command->peer, "192.168.50.3") == 0;
#endif
#else
    return strcmp(command->peer, TPUT_AP_IP) == 0 && validation_link_ready();
#endif
}

static int open_socket(const char *ip, uint16_t port)
{
    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (fd < 0) return -1;
    struct timeval timeout = { .tv_sec = 0, .tv_usec = TPUT_SOCKET_TIMEOUT_US };
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

static struct tput_header make_header(enum packet_type type, uint32_t stage,
                                      uint32_t seq, uint16_t packet_bytes)
{
    return (struct tput_header) {
        .magic = htonl(TPUT_MAGIC), .stage = htonl(stage), .seq = htonl(seq),
        .packet_bytes = htons(packet_bytes), .version = TPUT_VERSION, .type = type
    };
}

static bool header_valid(const struct tput_header *header)
{
    return header->magic == htonl(TPUT_MAGIC) && header->version == TPUT_VERSION;
}

static uint8_t body_byte(uint32_t stage, uint32_t seq, unsigned offset)
{
    return (uint8_t)(stage ^ (seq * 17U) ^ (offset * 31U) ^ (offset >> 4));
}

static void fill_data(uint8_t *packet, uint16_t packet_bytes, uint32_t stage, uint32_t seq)
{
    struct tput_header header = make_header(PKT_DATA, stage, seq, packet_bytes);
    memcpy(packet, &header, sizeof(header));
    for (unsigned i = sizeof(header); i < packet_bytes; ++i)
        packet[i] = body_byte(stage, seq, i - sizeof(header));
}

static bool data_valid(const uint8_t *packet, uint16_t packet_bytes,
                       uint32_t stage, uint32_t seq)
{
    for (unsigned i = sizeof(struct tput_header); i < packet_bytes; ++i) {
        if (packet[i] != body_byte(stage, seq, i - sizeof(struct tput_header))) return false;
    }
    return true;
}

static bool take_stage_command(uint32_t stage, struct tput_command *out)
{
    if (xQueueReceive(command_queue, out, 0) != pdTRUE) return false;
    if (out->type == COMMAND_STOP) {
        printf("RW_TPUT_CMD_ACK command=STOP\n");
        flush_control_log();
        return true;
    }
    if (out->stage != stage) {
        print_reject("stage_mismatch");
        return false;
    }
    if (out->type == COMMAND_ABORT) {
        printf("RW_TPUT_CMD_ACK command=ABORT stage=%" PRIu32 "\n", stage);
        flush_control_log();
        return true;
    }
    if (out->type == COMMAND_DRAIN) return true;
    print_reject("stage_busy");
    return false;
}

static void rx_sample(uint32_t stage, const struct rx_stats *stats, int64_t now)
{
    printf("RW_TPUT_RX_SAMPLE stage=%" PRIu32 " t_us=%" PRId64
           " active_unique=%" PRIu32 " late_unique=%" PRIu32
           " duplicate=%" PRIu32 " invalid=%" PRIu32 "\n",
           stage, now, stats->active_unique, stats->late_unique,
           stats->duplicate, stats->invalid);
}

static void rx_summary(const struct tput_command *command, const struct rx_stats *stats)
{
    const uint32_t all_unique = stats->active_unique + stats->late_unique;
    const uint32_t body = command->packet_bytes - sizeof(struct tput_header);
    const int64_t span = stats->last_active_us > stats->first_active_us
        ? stats->last_active_us - stats->first_active_us : 0;
    printf("RW_TPUT_RX_SUMMARY stage=%" PRIu32 " role=%s packet_bytes=%u body_bytes=%" PRIu32
           " active_unique=%" PRIu32 " late_unique=%" PRIu32 " drain_unique=%" PRIu32
           " all_unique=%" PRIu32 " active_body_bytes=%" PRIu64
           " all_body_bytes=%" PRIu64 " duplicate=%" PRIu32
           " invalid=%" PRIu32 " wrong_stage=%" PRIu32 " wrong_peer=%" PRIu32
           " overflow=%" PRIu32 " out_of_order=%" PRIu32
           " start_seen=%d end_seen=%d end_sent=%" PRIu32
           " drain_command=%d drain_sent=%" PRIu32 " count_match=%d"
           " start_us=%" PRId64 " end_us=%" PRId64
           " first_active_us=%" PRId64 " last_active_us=%" PRId64
           " active_span_us=%" PRId64 " drain_start_us=%" PRId64
           " drain_end_us=%" PRId64 " aborted=%d\n",
           command->stage,
#ifdef CONFIG_RW_LINK_AP
           "AP",
#else
           "STA",
#endif
           command->packet_bytes, body, stats->active_unique, stats->late_unique,
           stats->drain_unique, all_unique, (uint64_t)stats->active_unique * body,
           (uint64_t)all_unique * body, stats->duplicate, stats->invalid,
           stats->wrong_stage, stats->wrong_peer, stats->overflow,
           stats->out_of_order, stats->start_seen, stats->end_seen,
           stats->end_sent, stats->drain_command, stats->drain_sent,
           stats->drain_command && stats->end_seen && stats->drain_sent == stats->end_sent,
           stats->start_us, stats->end_us, stats->first_active_us,
           stats->last_active_us, span, stats->drain_start_us,
           stats->drain_end_us, stats->aborted);
    flush_control_log();
}

static enum stage_result receive_stage(const struct tput_command *command,
                                        int64_t session_deadline)
{
    char ip[16];
    if (!local_ip(ip, sizeof(ip)) || !allowed_peer(command)) {
        print_reject("ip_or_peer_not_ready");
        return STAGE_FAILED;
    }
    uint8_t *bitmap = heap_caps_malloc(TPUT_BITMAP_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    uint8_t *packet = malloc(TPUT_MAX_PACKET + 1);
    if (!bitmap || !packet) {
        free(bitmap);
        free(packet);
        print_reject("allocation");
        return STAGE_FAILED;
    }
    memset(bitmap, 0, TPUT_BITMAP_BYTES);
    int fd = open_socket(ip, TPUT_PORT);
    if (fd < 0) {
        printf("RW_TPUT_SOCKET_ERROR phase=rx_open errno=%d\n", errno);
        free(bitmap);
        free(packet);
        return STAGE_FAILED;
    }
    struct in_addr expected_ip;
    inet_pton(AF_INET, command->peer, &expected_ip);
    struct rx_stats stats = {0};
    uint16_t expected_port = 0;
    int64_t ready_us = esp_timer_get_time();
    int64_t sample_us = ready_us + TPUT_SAMPLE_US;
    int64_t last_idle_yield = ready_us;
    int64_t start_timeout = ready_us + TPUT_RX_START_TIMEOUT_US;
    int64_t max_deadline = ready_us + ((int64_t)command->duration_s + 35) * 1000000;
    printf("RW_TPUT_ARM_ACK stage=%" PRIu32 " peer=%s duration_s=%" PRIu32
           " packet_bytes=%u\n", command->stage, command->peer,
           command->duration_s, command->packet_bytes);
    printf("RW_TPUT_RX_READY stage=%" PRIu32 " role=%s ip=%s port=%d t_us=%" PRId64 "\n",
           command->stage,
#ifdef CONFIG_RW_LINK_AP
           "AP",
#else
           "STA",
#endif
           ip, TPUT_PORT, ready_us);
    flush_control_log();
    enum stage_result result = STAGE_OK;
    while (esp_timer_get_time() < session_deadline &&
           esp_timer_get_time() < max_deadline) {
        int64_t now = esp_timer_get_time();
        if (!stats.start_seen && now >= start_timeout) {
            stats.aborted = true;
            printf("RW_TPUT_STAGE_ABORT stage=%" PRIu32 " reason=start_timeout\n", command->stage);
            break;
        }
        if (stats.start_seen && !stats.drain_command &&
            now >= stats.start_us + ((int64_t)command->duration_s + 15) * 1000000) {
            stats.aborted = true;
            printf("RW_TPUT_STAGE_ABORT stage=%" PRIu32 " reason=drain_timeout\n", command->stage);
            break;
        }
        struct tput_command control;
        if (take_stage_command(command->stage, &control)) {
            if (control.type == COMMAND_STOP) {
                stats.aborted = true;
                result = STAGE_STOP;
                break;
            }
            if (control.type == COMMAND_ABORT) {
                stats.aborted = true;
                break;
            }
            if (control.type == COMMAND_DRAIN) {
                if (stats.drain_command) {
                    print_reject("duplicate_drain");
                } else {
                    stats.drain_command = true;
                    stats.drain_sent = control.sent; /* host's successful TX count */
                    stats.drain_start_us = esp_timer_get_time();
                    printf("RW_TPUT_CMD_ACK command=DRAIN stage=%" PRIu32
                           " sent=%" PRIu32 "\n", command->stage, control.sent);
                    printf("RW_TPUT_RX_DRAIN stage=%" PRIu32 " t_us=%" PRId64
                           " duration_ms=3000\n", command->stage, stats.drain_start_us);
                    flush_control_log();
                }
            }
        }
        now = esp_timer_get_time();
        if (stats.drain_command && now - stats.drain_start_us >= TPUT_DRAIN_US) {
            stats.drain_end_us = now;
            break;
        }
        struct sockaddr_in source = {0};
        socklen_t source_len = sizeof(source);
        int n = recvfrom(fd, packet, TPUT_MAX_PACKET + 1, 0,
                         (struct sockaddr *)&source, &source_len);
        now = esp_timer_get_time();
        if (n < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                printf("RW_TPUT_SOCKET_ERROR phase=rx_recv errno=%d\n", errno);
                stats.aborted = true;
                break;
            }
        } else if (source.sin_addr.s_addr != expected_ip.s_addr ||
                   (stats.start_seen && source.sin_port != expected_port)) {
            stats.wrong_peer++;
        } else if (n < (int)sizeof(struct tput_header)) {
            stats.invalid++;
        } else {
            struct tput_header header;
            memcpy(&header, packet, sizeof(header));
            if (!header_valid(&header)) {
                stats.invalid++;
            } else if (ntohl(header.stage) != command->stage) {
                stats.wrong_stage++;
            } else if (header.type == PKT_START && n == (int)sizeof(header) &&
                       ntohs(header.packet_bytes) == command->packet_bytes) {
                if (!stats.start_seen) {
                    stats.start_seen = true;
                    stats.start_us = now;
                    expected_port = source.sin_port;
                    printf("RW_TPUT_RX_START stage=%" PRIu32 " t_us=%" PRId64 "\n",
                           command->stage, now);
                }
                struct tput_header ack = make_header(PKT_ACK_START, command->stage, 0,
                                                      command->packet_bytes);
                (void)sendto(fd, &ack, sizeof(ack), 0,
                             (struct sockaddr *)&source, source_len);
            } else if (header.type == PKT_END && n == (int)sizeof(header) &&
                       stats.start_seen && ntohs(header.packet_bytes) == command->packet_bytes) {
                if (!stats.end_seen) {
                    stats.end_seen = true;
                    stats.end_us = now;
                    stats.end_sent = ntohl(header.seq);
                    printf("RW_TPUT_RX_END stage=%" PRIu32 " t_us=%" PRId64
                           " sent=%" PRIu32 "\n", command->stage, now, stats.end_sent);
                } else if (stats.end_sent != ntohl(header.seq)) {
                    stats.invalid++;
                }
                struct tput_header ack = make_header(PKT_ACK_END, command->stage,
                                                      stats.end_sent, command->packet_bytes);
                (void)sendto(fd, &ack, sizeof(ack), 0,
                             (struct sockaddr *)&source, source_len);
            } else if (header.type != PKT_DATA || !stats.start_seen ||
                       n != command->packet_bytes ||
                       ntohs(header.packet_bytes) != command->packet_bytes) {
                stats.invalid++;
            } else {
                uint32_t seq = ntohl(header.seq);
                if (seq >= TPUT_MAX_SEQ) {
                    stats.overflow++;
                } else if (stats.end_seen && seq >= stats.end_sent) {
                    stats.invalid++;
                } else if (!data_valid(packet, command->packet_bytes, command->stage, seq)) {
                    stats.invalid++;
                } else {
                    uint8_t mask = (uint8_t)(1U << (seq & 7));
                    uint8_t *byte = &bitmap[seq >> 3];
                    if (*byte & mask) {
                        stats.duplicate++;
                    } else {
                        *byte |= mask;
                        if (seq < stats.high_seq && !stats.drain_command) stats.out_of_order++;
                        if (seq >= stats.high_seq && !stats.drain_command) stats.high_seq = seq + 1;
                        if (stats.end_seen || stats.drain_command) {
                            stats.late_unique++;
                            if (stats.drain_command) stats.drain_unique++;
                        } else {
                            stats.active_unique++;
                            if (!stats.first_active_us) stats.first_active_us = now;
                            stats.last_active_us = now;
                        }
                    }
                }
            }
        }
        if (now >= sample_us) {
            rx_sample(command->stage, &stats, now);
            sample_us = now + TPUT_SAMPLE_US;
        }
        if (now - last_idle_yield >= 50000) {
            vTaskDelay(pdMS_TO_TICKS(1));
            last_idle_yield = esp_timer_get_time();
        }
    }
    if (!stats.drain_end_us) stats.drain_end_us = esp_timer_get_time();
    if (!stats.drain_command || !stats.start_seen || !stats.end_seen ||
        stats.drain_sent != stats.end_sent || stats.overflow ||
        stats.drain_end_us - stats.drain_start_us < TPUT_DRAIN_US) stats.aborted = true;
    rx_summary(command, &stats);
    close(fd);
    free(bitmap);
    free(packet);
    if (result == STAGE_STOP) return result;
    return stats.aborted ? STAGE_FAILED : STAGE_OK;
}

static bool control_exchange(int fd, enum packet_type request_type,
                             enum packet_type ack_type, uint32_t stage,
                             uint32_t seq, uint16_t packet_bytes,
                             int64_t session_deadline)
{
    struct tput_header request = make_header(request_type, stage, seq, packet_bytes);
    int64_t deadline = esp_timer_get_time() + TPUT_CTRL_TIMEOUT_US;
    if (deadline > session_deadline) deadline = session_deadline;
    while (esp_timer_get_time() < deadline) {
        (void)send(fd, &request, sizeof(request), 0);
        struct tput_header ack;
        int n = recv(fd, &ack, sizeof(ack), 0);
        if (n == (int)sizeof(ack) && header_valid(&ack) &&
            ack.type == ack_type && ntohl(ack.stage) == stage &&
            ntohl(ack.seq) == seq && ntohs(ack.packet_bytes) == packet_bytes) return true;
        if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK &&
            errno != EINTR && errno != ECONNREFUSED) break;
    }
    return false;
}

static void tx_sample(uint32_t stage, const struct tx_stats *stats, int64_t now)
{
    printf("RW_TPUT_TX_SAMPLE stage=%" PRIu32 " t_us=%" PRId64
           " attempts=%" PRIu32 " sent=%" PRIu32 " send_fail=%" PRIu32 "\n",
           stage, now, stats->attempts, stats->sent, stats->send_fail);
}

static void tx_summary(const struct tput_command *command, const struct tx_stats *stats)
{
    uint32_t body = command->packet_bytes - sizeof(struct tput_header);
    int64_t duration = stats->end_us > stats->start_us ? stats->end_us - stats->start_us : 0;
    printf("RW_TPUT_TX_SUMMARY stage=%" PRIu32 " role=%s peer=%s"
           " rate_kbps=%" PRIu32 " requested_ms=%" PRIu32
           " packet_bytes=%u body_bytes=%" PRIu32
           " attempts=%" PRIu32 " sent=%" PRIu32 " send_fail=%" PRIu32
           " sent_udp_bytes=%" PRIu64 " sent_body_bytes=%" PRIu64
           " start_acked=%d end_acked=%d start_us=%" PRId64
           " end_us=%" PRId64 " duration_us=%" PRId64 " aborted=%d\n",
           command->stage,
#ifdef CONFIG_RW_LINK_AP
           "AP",
#else
           "STA",
#endif
           command->peer, command->rate_kbps, command->duration_s * 1000,
           command->packet_bytes, body, stats->attempts, stats->sent,
           stats->send_fail, (uint64_t)stats->sent * command->packet_bytes,
           (uint64_t)stats->sent * body, stats->start_acked, stats->end_acked,
           stats->start_us, stats->end_us, duration, stats->aborted);
    flush_control_log();
}

static enum stage_result send_stage(const struct tput_command *command,
                                     int64_t session_deadline)
{
    char ip[16];
    if (!local_ip(ip, sizeof(ip)) || !allowed_peer(command)) {
        print_reject("ip_or_peer_not_ready");
        return STAGE_FAILED;
    }
    uint8_t *packet = malloc(command->packet_bytes);
    if (!packet) {
        print_reject("allocation");
        return STAGE_FAILED;
    }
    int fd = open_socket(ip, 0);
    if (fd < 0) {
        printf("RW_TPUT_SOCKET_ERROR phase=tx_open errno=%d\n", errno);
        free(packet);
        return STAGE_FAILED;
    }
    struct sockaddr_in peer = { .sin_family = AF_INET, .sin_port = htons(TPUT_PORT) };
    inet_pton(AF_INET, command->peer, &peer.sin_addr);
    if (connect(fd, (struct sockaddr *)&peer, sizeof(peer)) != 0) {
        printf("RW_TPUT_SOCKET_ERROR phase=tx_connect errno=%d\n", errno);
        close(fd);
        free(packet);
        return STAGE_FAILED;
    }
    printf("RW_TPUT_SEND_ACK stage=%" PRIu32 " peer=%s duration_s=%" PRIu32
           " rate_kbps=%" PRIu32 " packet_bytes=%u\n", command->stage,
           command->peer, command->duration_s, command->rate_kbps, command->packet_bytes);
    flush_control_log();
    struct tx_stats stats = {0};
    enum stage_result result = STAGE_OK;
    stats.start_acked = control_exchange(fd, PKT_START, PKT_ACK_START, command->stage,
                                          0, command->packet_bytes, session_deadline);
    if (!stats.start_acked) {
        stats.aborted = true;
        printf("RW_TPUT_STAGE_ABORT stage=%" PRIu32 " reason=start_ack_timeout\n", command->stage);
        goto done;
    }
    stats.start_us = esp_timer_get_time();
    int64_t target_end = stats.start_us + (int64_t)command->duration_s * 1000000;
    int64_t next_send = stats.start_us;
    int64_t sample_us = stats.start_us + TPUT_SAMPLE_US;
    int64_t last_idle_yield = stats.start_us;
    const int64_t period_us = command->rate_kbps
        ? ((int64_t)command->packet_bytes * 8000 + command->rate_kbps - 1) /
          command->rate_kbps : 0;
    printf("RW_TPUT_TX_START stage=%" PRIu32 " t_us=%" PRId64 "\n",
           command->stage, stats.start_us);
    flush_control_log();
    while (esp_timer_get_time() < target_end && esp_timer_get_time() < session_deadline) {
        struct tput_command control;
        if (take_stage_command(command->stage, &control)) {
            if (control.type == COMMAND_STOP) result = STAGE_STOP;
            else if (control.type == COMMAND_ABORT) result = STAGE_FAILED;
            else print_reject("drain_on_sender");
            if (result != STAGE_OK) { stats.aborted = true; break; }
        }
        int64_t now = esp_timer_get_time();
        if (period_us && now < next_send) {
            int64_t remaining = next_send - now;
            if (remaining >= 2000) vTaskDelay(pdMS_TO_TICKS(1));
            else taskYIELD();
            continue;
        }
        if (stats.sent >= TPUT_MAX_SEQ) {
            stats.aborted = true;
            printf("RW_TPUT_STAGE_ABORT stage=%" PRIu32 " reason=seq_capacity\n", command->stage);
            break;
        }
        int64_t attempt_us = now;
        fill_data(packet, command->packet_bytes, command->stage, stats.sent);
        stats.attempts++;
        int n = send(fd, packet, command->packet_bytes, 0);
        if (n == command->packet_bytes) {
            stats.sent++;
        } else {
            stats.send_fail++;
            if (stats.send_fail <= 3)
                printf("RW_TPUT_SEND_ERROR stage=%" PRIu32 " errno=%d\n",
                       command->stage, errno);
            vTaskDelay(pdMS_TO_TICKS(1));
        }
        now = esp_timer_get_time();
        /* Period includes packet construction and send time. If a send takes
         * longer than a slot, skip the missed slot instead of bursting. */
        if (period_us) next_send = attempt_us + period_us > now
            ? attempt_us + period_us : now;
        /* taskYIELD alone does not run lower-priority IDLE/WDT tasks. */
        if (now - last_idle_yield >= 50000) {
            vTaskDelay(pdMS_TO_TICKS(1));
            last_idle_yield = esp_timer_get_time();
        }
        if (now >= sample_us) {
            tx_sample(command->stage, &stats, now);
            sample_us = now + TPUT_SAMPLE_US;
        }
    }
    stats.end_us = esp_timer_get_time();
    printf("RW_TPUT_TX_END stage=%" PRIu32 " t_us=%" PRId64
           " sent=%" PRIu32 "\n", command->stage, stats.end_us, stats.sent);
    flush_control_log();
    if (stats.end_us + 100000 < target_end) stats.aborted = true;
    if (result == STAGE_OK && !stats.aborted) {
        stats.end_acked = control_exchange(fd, PKT_END, PKT_ACK_END, command->stage,
                                            stats.sent, command->packet_bytes, session_deadline);
        if (!stats.end_acked) {
            stats.aborted = true;
            printf("RW_TPUT_STAGE_ABORT stage=%" PRIu32 " reason=end_ack_timeout\n",
                   command->stage);
        }
    }
done:
    tx_summary(command, &stats);
    close(fd);
    free(packet);
    if (result == STAGE_STOP) return result;
    return stats.aborted ? STAGE_FAILED : STAGE_OK;
}

bool run_throughput(void)
{
    char ip[16] = {0};
    int64_t wait_end = esp_timer_get_time() + 30000000LL;
    while (esp_timer_get_time() < wait_end && !local_ip(ip, sizeof(ip)))
        vTaskDelay(pdMS_TO_TICKS(100));
    if (!ip[0]) {
        printf("RW_TPUT_SESSION_ABORT reason=ip_timeout\n");
        return false;
    }
    command_queue = xQueueCreate(8, sizeof(struct tput_command));
    if (!command_queue || xTaskCreate(read_commands, "rw_tput_cmd", 4096, NULL,
                                      4, &command_task) != pdPASS) {
        if (command_queue) vQueueDelete(command_queue);
        command_queue = NULL;
        printf("RW_TPUT_SESSION_ABORT reason=command_reader\n");
        return false;
    }
    int64_t begin_us = esp_timer_get_time();
    int64_t deadline = begin_us + (int64_t)CONFIG_RW_TPUT_SESSION_SECONDS * 1000000;
    printf("RW_TPUT_SESSION_START role=%s id=%d ip=%s port=%d session_s=%d"
           " t_us=%" PRId64 " sdk_log=WARN\n",
#ifdef CONFIG_RW_LINK_AP
           "AP",
#else
           "STA",
#endif
           CONFIG_RW_LINK_STA_ID, ip, TPUT_PORT, CONFIG_RW_TPUT_SESSION_SECONDS, begin_us);
#ifdef CONFIG_RW_LINK_AP
    printf("RW_LINK_AP_READY ip=%s port=%d window_s=%d\n",
           ip, TPUT_PORT, CONFIG_RW_TPUT_SESSION_SECONDS);
#endif
    printf("RW_TPUT_READY role=%s ip=%s port=%d t_us=%" PRId64 "\n",
#ifdef CONFIG_RW_LINK_AP
           "AP",
#else
           "STA",
#endif
           ip, TPUT_PORT, begin_us);
    flush_control_log();
    bool all_ok = true, stopped = false;
    uint32_t last_stage = 0, stages = 0;
    while (esp_timer_get_time() < deadline) {
        struct tput_command command;
        if (xQueueReceive(command_queue, &command, pdMS_TO_TICKS(100)) != pdTRUE) continue;
        if (command.type == COMMAND_STOP) {
            printf("RW_TPUT_CMD_ACK command=STOP\n");
            flush_control_log();
            stopped = true;
            break;
        }
        if (command.type != COMMAND_ARM && command.type != COMMAND_SEND) {
            print_reject("no_active_stage");
            continue;
        }
        if (command.stage <= last_stage) {
            print_reject("stage_not_increasing");
            continue;
        }
        last_stage = command.stage;
        stages++;
        enum stage_result result = command.type == COMMAND_ARM
            ? receive_stage(&command, deadline) : send_stage(&command, deadline);
        if (result != STAGE_OK) all_ok = false;
        if (result == STAGE_STOP) { stopped = true; break; }
    }
    if (!stopped) {
        all_ok = false;
        printf("RW_TPUT_SESSION_ABORT reason=deadline\n");
    }
    printf("RW_TPUT_SESSION_END role=%s stages=%" PRIu32 " all_stages_ok=%d"
           " stopped=%d elapsed_ms=%" PRId64 "\n",
#ifdef CONFIG_RW_LINK_AP
           "AP",
#else
           "STA",
#endif
           stages, all_ok, stopped, (esp_timer_get_time() - begin_us) / 1000);
    flush_control_log();
    vTaskDelete(command_task);
    command_task = NULL;
    vQueueDelete(command_queue);
    command_queue = NULL;
    return all_ok && stopped && stages > 0;
}
#endif
