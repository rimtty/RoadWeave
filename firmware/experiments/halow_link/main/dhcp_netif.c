#include "sdkconfig.h"
#if defined(CONFIG_RW_LINK_DHCP) && defined(CONFIG_RW_LINK_AP)

#include <stdbool.h>
#include <string.h>
#include "esp_netif.h"
#include "esp_netif_defaults.h"

/* The pinned mmhalow_init() always requests WIFI_STA_DEF. Its RX/link callbacks
 * retain that very netif, so changing its creation config is necessary to give
 * the AP a DHCP-server-capable lwIP stack. Guard the wrapper strictly around
 * mmhalow_init(); unrelated esp_netif_new() calls retain their original config. */
static bool wrap_mmhalow_netif;

void validation_dhcp_wrap_begin(void)
{
    wrap_mmhalow_netif = true;
}

void validation_dhcp_wrap_end(void)
{
    wrap_mmhalow_netif = false;
}

esp_netif_t *__real_esp_netif_new(const esp_netif_config_t *config);

esp_netif_t *__wrap_esp_netif_new(const esp_netif_config_t *config)
{
    if (!wrap_mmhalow_netif || !config || !config->base ||
        !config->base->if_key || strcmp(config->base->if_key, "WIFI_STA_DEF") != 0) {
        return __real_esp_netif_new(config);
    }
    esp_netif_inherent_config_t base = *config->base;
    esp_netif_config_t ap_config = *config;
    base.flags = (esp_netif_flags_t)((base.flags &
        ~(ESP_NETIF_DHCP_CLIENT | ESP_NETIF_FLAG_EVENT_IP_MODIFIED | ESP_NETIF_FLAG_AUTOUP)) |
        ESP_NETIF_DHCP_SERVER);
    base.get_ip_event = 0;
    base.lost_ip_event = 0;
    base.if_desc = "halow_ap";
    ap_config.base = &base;
    ap_config.stack = ESP_NETIF_NETSTACK_DEFAULT_WIFI_AP;
    return __real_esp_netif_new(&ap_config);
}

#endif
