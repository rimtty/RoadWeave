#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
bool run_preflight(void);
bool run_radio_test(void);
#ifdef CONFIG_RW_LINK_CONTINUOUS
struct mmwlan_ap_args;
bool run_validation_ap(const struct mmwlan_ap_args *ap);
bool run_validation_sta(void);
#ifdef CONFIG_RW_LINK_THROUGHPUT
bool run_throughput(void);
bool validation_report_operating_channel(void);
#endif
bool validation_link_ready(void);
bool validation_sta_ip(char *out, size_t out_len);
bool validation_ap_netif_up(void);
bool validation_ap_netif_down(void);
bool validation_ap_lease_mac(uint32_t ip_addr, uint8_t mac[6]);
bool validation_restart_requested(void);
bool validation_radio_shutdown_ok(void);
#if defined(CONFIG_RW_LINK_DHCP) && defined(CONFIG_RW_LINK_AP)
void validation_dhcp_wrap_begin(void);
void validation_dhcp_wrap_end(void);
#endif
#endif
enum rw_led_mode { RW_LED_WAIT, RW_LED_CONNECTED, RW_LED_PASS, RW_LED_FAIL };
void status_led_init(void);
void status_led_set(enum rw_led_mode mode);
void status_led_activity(void);
