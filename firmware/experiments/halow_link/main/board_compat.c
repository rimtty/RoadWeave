#include "sdkconfig.h"
#include "mmhal_wlan.h"

/* Wio-WM6180 leaves BUSY/WAKE disconnected and pulls chip WAKE high.
 * In SDK 2.11.2-esp32-2, the no-power-save shim returns false for BUSY.
 * The transport then disables SPI IRQ while waiting for an unwired BUSY IRQ
 * to wake it. Report the always-awake board as busy so receive IRQ stays on.
 * This is a transport sleep veto, not a physical GPIO measurement.
 * Restrict the override to this board's explicitly RF-enabled experiment. */
bool __real_mmhal_wlan_busy_is_asserted(void);
bool __wrap_mmhal_wlan_busy_is_asserted(void)
{
#if defined(CONFIG_RW_LINK_RF_ENABLE) && !defined(CONFIG_HALOW_PS_MODE)
    return true;
#else
    return __real_mmhal_wlan_busy_is_asserted();
#endif
}
