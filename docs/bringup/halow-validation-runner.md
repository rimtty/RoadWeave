# Finite HaLow serial validation runner

`tools/halow_validation_run.py` captures COM4 AP with COM5 STA, COM6 STA, or
both stations through the ESP32-S3 native USB serial ports. It supports the
continuous MM6108 SPI HaLow validation firmware, not the older one-shot
20-probe image. Three-node mode requires STA IDs 1 and 2, distinct addresses,
and AP capacity for two stations. The optional `--sta2 COM6` selects it;
without that flag the established two-node path is unchanged.

Use the ESP-IDF Python environment with `pyserial` and `esp_idf_monitor` installed.
Before a hardware run, build/flash AP and selected STAs with matching SSID/PSK in local
ignored sdkconfig. Select continuous mode, a finite firmware run duration longer
than the host observation, and the intended channel width. For the baseline,
use 256-byte probes at 250 ms, observe at least 120 s, and leave room for the
runner's STOP command. The baseline discards the first 10 s using the first
sample at or after that point, then requires 120 s of observation and 200 sent
probes. Keep the PSK and complete raw logs under `.private`.

Three-node baseline example (STA ID 1 on COM5, STA ID 2 on COM6):

```powershell
& D:/Espressif/tools/python_env/idf5.4_py3.12_env/Scripts/python.exe tools/halow_validation_run.py `
  --sta COM5 --sta2 COM6 --seconds 150 `
  --output-dir .private/halow-validation/three-node-baseline `
  --ap-bin firmware/experiments/halow_link/build/ap/roadweave_halow_link.bin `
  --sta-bin firmware/experiments/halow_link/build/sta/roadweave_halow_link.bin `
  --sta2-bin firmware/experiments/halow_link/build/sta2/roadweave_halow_link.bin
```

Both station reports need at least 200 successful sends and 99% exact echoes
after the first 10 s, plus a common 120 s measurement window with at least
200 exact echoes from each station. The AP's `echo_id1` and `echo_id2` counts
must cover every exact reply during the AP's final run, including replies
before a station restart. For DHCP builds the analyzer
uses each station's lease and UDP bind address, checks they differ, and checks
the AP's peer ID/address mapping. DHCP evidence must come from the current
boot; software resets require fresh lease/bind evidence. The report identifies
static or DHCP mode.

```powershell
& D:/Espressif/tools/python_env/idf5.4_py3.12_env/Scripts/python.exe tools/halow_validation_run.py `
  --sta COM5 --seconds 150 `
  --output-dir .private/halow-validation/com5-width1-run1 `
  --ap-bin firmware/experiments/halow_link/build/ap/roadweave_halow_link.bin `
  --sta-bin firmware/experiments/halow_link/build/sta/roadweave_halow_link.bin `
  --public-log docs/bringup/logs/com5-width1-run1.log
```

The runner verifies the selected USB serial-number MAC against the known COM4,
COM5 and COM6 inventory. It holds DTR/RTS deasserted, applies an initial ESP-IDF
serial reset to AP, waits for a fresh `RW_LINK_AP_READY`, and only then resets
the selected stations. That serial reset is **not** a physical cold boot. `--no-initial-reset`
is available when the firmware is already running, but a fresh AP_READY still
must be captured. No USB serial tool can establish the 50 physical power-cycle
criterion; record those cycles separately with a controlled power fixture.

To exercise faults, repeat `--fault ROLE:ACTION:OFFSET_SECONDS` within a
bounded host run. Available actions are `software_reset` for either role and
`ap_off_10s` for AP. For example, three AP software resets:

```powershell
& D:/Espressif/tools/python_env/idf5.4_py3.12_env/Scripts/python.exe tools/halow_validation_run.py `
  --sta COM5 --seconds 240 `
  --fault ap:software_reset:30 --fault ap:software_reset:90 --fault ap:software_reset:150 `
  --output-dir .private/halow-validation/com5-ap-reset
```

Run STA resets and `ap_off_10s` in separate captures, using three offsets each.
In three-node mode, `--fault sta:software_reset:30` or
`--fault sta2:software_reset:30` resets one station. The other station must
keep producing exact echoes during the restart, with no outage marker and no
inter-echo gap over 5 s across that window. The restarted station must resume
within 60 s of its new boot and record 20 consecutive exact echoes.
Keep both firmware role durations longer than the observation plus any restart
delay. At the host deadline the runner sends `RW_LINK_CMD STOP` to active roles
and waits up to 20 s for final `RW_LINK_SUMMARY`, radio result, radio shutdown,
and `RW_LINK_DONE`. A timeout or interrupted capture fails. The command's ACK,
new boot marker, AP_READY, valid echoes, recovery windows, and heap samples are
saved with host UTC and monotonic timestamps. After each fault, the acceptance
check requires a valid echo within 60 s of the AP_READY or STA boot and 20
consecutive exact echoes. The baseline needs at least 200 successfully sent
probes, at least 99% exact echoes among them, and at least 120 s observed from
the first sample after the 10 s warmup to summary. The one-second probe timeout and skipped probes are
reported separately from successfully sent probes.

Each private output directory contains `ap-raw.log`, `sta-raw.log`, and, in
three-node mode, `sta2-raw.log`, plus
`events.jsonl`, `report.json`, and `report.md`. The raw logs preserve driver
warnings. The JSON report counts `Address base set failed` and `Unknown TLV`
by role, boot index and raw line; the former is a propagated transport error,
while the latter is skipped during TLV parsing. Review their local context
before classifying either. The optional public log includes allowlisted
`RW_LINK_*` fields only and removes PSK/SSID. Check it before committing.
Serial command writes time out after 2 s. `events.jsonl` is flushed after each
event, so a forced termination still leaves host fault times and parsed serial
evidence; an ACK that was not finalized may remain conservatively unacknowledged.
An interrupted run without final summaries, shutdown markers, and `RW_LINK_DONE`
cannot pass even if its raw logs contain echoes.
Scan-time SNR is retained only when firmware reports valid RSSI and noise.
`NA` stays unknown, and negative SNR is preserved. The UDP summary's
`snr_db=NA` is not replaced with the scan-time value.

For offline reanalysis of an event file:

```powershell
& D:/Espressif/tools/python_env/idf5.4_py3.12_env/Scripts/python.exe tools/halow_validation.py `
  .private/halow-validation/com5-width1-run1/events.jsonl `
  --json .private/halow-validation/com5-width1-run1/reanalysis.json `
  --markdown .private/halow-validation/com5-width1-run1/reanalysis.md
```

Offline parser tests require no serial device:

```powershell
& D:/Espressif/tools/python_env/idf5.4_py3.12_env/Scripts/python.exe -m unittest discover -s tools/tests -p test_halow_validation.py
```
