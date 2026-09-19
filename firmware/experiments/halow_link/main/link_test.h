#pragma once
#include <stdbool.h>
bool run_preflight(void);
bool run_radio_test(void);
#ifdef CONFIG_RW_LINK_CONTINUOUS
#include "mmhalow.h"
bool run_validation_ap(const struct mmwlan_ap_args *ap);
bool run_validation_sta(void);
bool validation_link_ready(void);
void validation_ap_netif_up(void);
#endif
enum rw_led_mode { RW_LED_WAIT, RW_LED_CONNECTED, RW_LED_PASS, RW_LED_FAIL };
void status_led_init(void);
void status_led_set(enum rw_led_mode mode);
void status_led_activity(void);
