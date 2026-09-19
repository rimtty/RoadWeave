# XIAO + Wio-WM6180 two-node link bench

ESP-IDF **v5.4.4**, `morsemicro/halow` **2.11.2-esp32-2**. Based on the pinned
official `softap` / `sta_connect` examples and Porting Assistant SPI transport.
This project uses only the SPI-connected MM6108 for wireless communication.
It does not initialize ESP32 internal 2.4 GHz Wi-Fi or use a 2.4/5 GHz WLAN link.

## Default: no RF transmission

`RW_LINK_RF_ENABLE` defaults to **off**. Both roles run the same preflight:

- Read the embedded firmware and BCF TLV containers through their EOF markers.
  This checks file structure, not calibration correctness or radio startup.
- Initialize SPI, read MM6108 ID 100 times with transport CRC validation.
- Deinitialize the bus and hold WM6180 RESET_N low.
- Print `RW_LINK_PREFLIGHT=PASS/FAIL`, `RW_LINK_RADIO_RESULT=NOT_RUN`, `RW_LINK_DONE`.

Preflight never calls `mmhalow_init`, `mmwlan_boot`, scanning, association, or UDP.
BUSY/WAKE are unwired on the standard WM6180; `HALOW_PS_MODE` must stay disabled.
The driver disables chip power save after radio boot in an RF-enabled build.
The board adapter additionally vetoes transport sleep: this board has WAKE pulled
high and no BUSY wake interrupt. Returning an always-busy indication keeps SPI RX
interrupts enabled; it is not a measurement of the physical BUSY pin. This adapter
is scoped to the RF-enabled experiment and the pinned SDK's no-power-save shim.

## Build and run preflight (PowerShell)

From the repository root:

```powershell
. ./firmware/scripts/enter-idf.ps1
Set-Location firmware/experiments/halow_link
idf.py -B build/ap -D SDKCONFIG=build/ap/sdkconfig -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.ap' build
idf.py -B build/sta -D SDKCONFIG=build/sta/sdkconfig -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.sta' build
```

Identify the current USB ports by ESP32 MAC before flashing. Preserve each device's
existing Flash first; the September 8 inventory already has verified full backups.
After verifying that `RW_LINK_RF_ENABLE` is off in the corresponding generated
`build/<role>/sdkconfig`, flash the selected role with
`idf.py -B build/<role> -p COMx flash`. These partition offsets preserve NVS.
Use `tools/serial_capture.py --reset --until RW_LINK_DONE` from the repository root
to capture a bounded preflight run. The diagnostic firmware remains installed.

## RF-enabled AP / STA test (requires the RF setup)

Do not transmit into an open WM6180 RF connector or connect two RF ports directly
with an unattenuated cable. Follow the project's [RF setup requirements](../../../docs/bringup/xiao-wm6180-first-boot.md).
US country selection describes this module variant; it is not authorization for
radiated operation in Japan. Confirm the applicable operating conditions before
using antennas, or prepare an appropriate conducted/shielded setup.

For each role, use `idf.py -B build/<role> menuconfig`. Under RoadWeave HaLow link
test, enable RF, choose a valid channel and operating class for the test setup,
and configure the same SSID and WPA3 passphrase on both devices. The PSK is blank
by default; keep credentials in ignored generated sdkconfig files, never in Git.
Rebuild after configuration. Changing defaults does not override an existing sdkconfig.
`RW_LINK_TX_POWER_DBM` defaults to a 1 dBm maximum-power override (not a measured
output power). Zero is deliberately excluded because it disables the SDK override.
Both roles restrict operation/scanning to the configured channel from the US
regulatory database; use its S1G operating class.

- AP: static `192.168.50.1/24`, WPA3-SAE, one STA, UDP port 3333, 120-second echo window.
- STA: static `192.168.50.2/24`, waits up to 30 seconds for both association and IPv4 readiness,
  then sends 20 probes. Exact echoed nonce/sequence payloads count as received;
  wrong, stale or duplicate replies do not count.
- Start/reset AP first and wait for `RW_LINK_AP_READY`, then start/reset STA.
- STA reports sent/received counts, mean/max RTT (microseconds), and
  `RW_LINK_RADIO_RESULT=PASS` only for 20/20 exact echoes.
- AP's result covers receiving/echoing at least 20 packets; the STA result is the
  end-to-end measurement. AP stays up for its full window, then shuts down.
- Both stop the radio when their test ends. Socket receives/sends are bounded.
- DHCP, reconnection, long-duration soak and throughput saturation are separate tests.

## Finite continuous validation mode

Enable `RW_LINK_CONTINUOUS` in both RF-enabled builds to run a bounded UDP echo
validation instead of the legacy 20-probe test. Set `RW_LINK_RUN_SECONDS` for each
role (for example AP 240, STA 180), `RW_LINK_PAYLOAD_BYTES` (default 256),
`RW_LINK_INTERVAL_MS` (default 250, STA only), and optionally
`RW_LINK_MAX_PROBES` (0 means use the duration limit). Keep the AP duration long
enough for STA startup and the intended observation. The configured duration is
at most eight hours; this mode does not itself constitute an eight-hour soak.

The AP accepts up to two STAs. Select `RW_LINK_STA_ID=1` for `192.168.50.2/24`
or `RW_LINK_STA_ID=2` for `192.168.50.3/24` when building each STA. Never boot
two STAs with the same ID/IP at once. The AP checks each datagram's source IP
against its ID and reports separate echo counts. These are still static IP
addresses; DHCP requires a separate netif change.

Each run prints `RW_LINK_BOOT`, `RW_LINK_RUN_START`, ten-second cumulative
`RW_LINK_SAMPLE`, per-probe `RW_LINK_ECHO_MATCH` or `RW_LINK_TIMEOUT`, final
`RW_LINK_SUMMARY`, `RW_LINK_RUN_END`, and the existing shutdown/DONE markers.
The STA summary includes planned slots, successful sends, matching replies,
lost sends, skipped slots, send failures, stale/duplicate/invalid replies,
throughput, heap, RSSI, and raw rate-control counters. `offered_bps` counts
successfully sent UDP payload bits over elapsed time; `useful_bps` counts only
matched echoed payload bits. This stop-and-wait workload is a link baseline,
not a maximum-throughput test. RTT percentiles in firmware use 1 ms histogram
bins and round down; `RW_LINK_ECHO_MATCH` carries each exact RTT for host
percentile calculations. Rate-control `rc_sent_start/end` and
`rc_success_start/end` are raw SDK counters, not a retry metric; SNR is `NA`
because this wrapper does not expose a validated SNR measurement.
The duplicate counter recognizes another reply to the most recently matched
sequence; older delayed replies count as stale (`late`). A successful
`RW_LINK_RADIO_RESULT` and LED mean the run completed with at least one valid
echo; `RW_LINK_RUN_END quality_gate=host` marks that quality gates are applied
by the host report.

The STA remains enabled across a link drop, letting the pinned Morse supplicant
reconnect. After a previously working link stops answering, the first matching
echo prints `RW_LINK_RECOVERY` with time since detection. It measures application
recovery, which can lag the physical link event. A run with some packet loss can
still report `RW_LINK_RADIO_RESULT=PASS`; use the summary and test-plan thresholds
to judge link quality.

While this mode is active, send a newline-terminated command to the USB
Serial/JTAG console: `RW_LINK_CMD RESTART` acknowledges and performs a software
reset; `RW_LINK_CMD AP_OFF_10S` on the AP disables its service for ten seconds,
reenables it and emits a new `RW_LINK_AP_READY`; `RW_LINK_CMD STOP` prints the
final summary and shuts down the radio. The AP outage is a controlled service
interruption, not a calibrated RF propagation loss. Software reset is reported
by `RW_LINK_BOOT reset_reason` and must not be counted as a cold power cycle.

## User LED

The [XIAO ESP32S3 user LED](https://wiki.seeedstudio.com/xiao-esp32s3-freertos/)
uses GPIO21, active low, separate from the HaLow SPI pins.

| Pattern | Meaning |
|---|---|
| Slow blink (500 ms on / 500 ms off) | Starting or waiting for a peer |
| On | STA connected / AP has an authorized STA |
| Brief off pulse (80 ms) | A valid UDP echo was received/sent |
| Two flashes every 2 seconds | Test completed successfully; radio stopped |
| Three flashes every 1.6 seconds | Test failed; inspect serial log |

The final pattern remains until reset. In preflight mode, the result pattern
describes preflight only; it does not indicate a radio connection.
Resetting an RF-enabled build starts another test automatically.

## Current validation status

The September 19 retest established HaLow association and UDP echo after correcting
channel setup ordering, vetoing transport sleep on the unwired-BUSY board, and
providing the lwIP transmit-wrap callback alongside explicit TX VIF selection.
See the [retest record](../../../docs/bringup/halow-link-retest-2026-09-19.md) for
the tested device pairs, results and remaining limits. Earlier failed RF attempts
remain in the [September 8 record](../../../docs/bringup/halow-link-antenna-test-2026-09-08.md).
The experiment is separate from production firmware. Preflight success is recorded
[separately](../../../docs/bringup/halow-link-preflight-2026-09-08.md).
