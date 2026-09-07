#include <stdio.h>
#include <string.h>
#include "mmhal.h"
#include "sdio_spi.h"
#include "link_test.h"

typedef void (*read_file_fn)(uint32_t, uint32_t, struct mmhal_robuf *);

static bool read_bytes(read_file_fn read_fn, uint32_t offset, void *output, uint32_t len)
{
    struct mmhal_robuf data = {0};
    read_fn(offset, len, &data);
    bool ok = data.buf && data.len == len;
    if (ok && output) memcpy(output, data.buf, len);
    if (data.free_cb) data.free_cb(data.free_arg);
    return ok;
}

/* Check container structure only. This does not validate RF calibration or boot firmware. */
static bool check_file(const char *name, read_file_fn read_fn, uint32_t magic)
{
    uint32_t offset = 0;
    for (unsigned tlv = 0; tlv < 256; ++tlv) {
        uint8_t h[4];
        if (!read_bytes(read_fn, offset, h, sizeof(h))) return false;
        unsigned type = h[0] | (h[1] << 8), len = h[2] | (h[3] << 8);
        if (tlv == 0 && (type != 0x8000 || len < 4)) return false;
        if (type == 0x8f00) {
            printf("RW_LINK_%s_CONTAINER=PASS bytes_before_eof=%lu tlvs=%u\n",
                   name, (unsigned long)offset, tlv);
            return true;
        }
        offset += 4;
        if (offset + len > 2 * 1024 * 1024) return false;
        if (tlv == 0) {
            uint8_t m[4];
            if (!read_bytes(read_fn, offset, m, 4)) return false;
            uint32_t found = (uint32_t)m[0] | (uint32_t)m[1] << 8 |
                             (uint32_t)m[2] << 16 | (uint32_t)m[3] << 24;
            if (found != magic) return false;
        }
        while (len) {
            unsigned n = len > 256 ? 256 : len;
            if (!read_bytes(read_fn, offset, NULL, n)) return false;
            offset += n;
            len -= n;
        }
    }
    return false;
}

bool run_preflight(void)
{
    mmhal_init();
    mmhal_set_deep_sleep_veto(MMHAL_VETO_ID_APP_MIN);
    bool fw = check_file("FW", mmhal_wlan_read_fw_file, 0x57464d4d);
    bool bcf = check_file("BCF", mmhal_wlan_read_bcf_file, 0x43424d4d);
    printf("RW_LINK_FILES=%s\n", fw && bcf ? "PASS" : "FAIL");
    mmhal_wlan_init();
    mmhal_wlan_hard_reset();
    int ret = mmhal_wlan_sdio_startup();
    uint32_t id = 0;
    if (!ret) {
        for (unsigned tries = 0; tries < 3; ++tries) {
            ret = sdio_spi_read_le32(0x10054d20, &id);
            if (!ret) break;
        }
    }
    unsigned matched = 0;
    if (!ret && (id == 0x206 || id == 0x306 || id == 0x406)) {
        for (; matched < 100; ++matched) {
            uint32_t again = 0;
            ret = sdio_spi_read_le32(0x10054d20, &again);
            if (ret || again != id) break;
        }
    }
    printf("RW_LINK_SPI=%s chip_id=0x%08lx matched=%u/100 ret=%d\n",
           matched == 100 ? "PASS" : "FAIL", (unsigned long)id, matched, ret);
    mmhal_wlan_deinit();
    printf("RW_LINK_RESET_N=LOW; BUSY/WAKE not used for power save\n");
    return fw && bcf && matched == 100;
}
