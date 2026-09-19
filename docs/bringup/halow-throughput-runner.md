# Bounded HaLow UDP throughput runner

`tools/halow_throughput_run.py` captures one direction and one channel width per
run. It uses COM4/AP with COM5/STA, checks both native USB serial identities,
starts the AP before the STA, and requires each role's managed throughput
session and the STA's fresh DHCP lease. The 1200-byte UDP datagram contains a
16-byte test header and 1184 bytes of checked application body. The program
never switches radios or channel width; flash the corresponding AP and STA
binaries before each width's captures.

Run it from this repository so `host_git` records the host tool revision. The
binary paths must name the actual flashed images. Use a new output directory
under `.private` for every invocation:

```powershell
& D:/Espressif/tools/python_env/idf5.4_py3.12_env/Scripts/python.exe -B tools/halow_throughput_run.py `
  --direction sta_to_ap --bandwidth-mhz 1 --sta COM5 `
  --ap-bin D:/RoadWeave/.private/halow-throughput-20260919/firmware-41929f5/tput-1mhz-ap.bin `
  --sta-bin D:/RoadWeave/.private/halow-throughput-20260919/firmware-41929f5/tput-1mhz-sta.bin `
  --fw-source-commit 41929f545b5ca70e434363f7edc0d84584270669 `
  --output-dir D:/RoadWeave/.private/halow-throughput-20260919/1mhz-sta-to-ap
```

For the reverse direction, pass `--direction ap_to_sta` and a fresh output
directory. Repeat both directions with the 2 MHz binaries and
`--bandwidth-mhz 2`. The default finite policy warms up for 5 seconds, tests
128/256/512/1000/2000/4000/8000 kbit/s for 30 seconds each until the first
quality failure, refines the boundary with two midpoint tests, attempts three
60-second confirmations at the highest candidate and lower candidates if
needed, then runs one unpaced saturation observation. The host caps its
session at 1500 seconds; the firmware session watchdog is 1800 seconds.
`--smoke --rates-kbps 128 --warmup-seconds 5 --stage-seconds 5
--repeat-seconds 5 --max-seconds 180` checks the protocol without unpaced
load and reports `SMOKE_PASS` instead of a full throughput result.

Every stage ARM's the receiver before SEND. The host waits for the TX summary,
then commands a three-second receiver DRAIN with the exact TX successful-send
count. It checks both summaries, START/END acknowledgments, device-clock DATA
duration, receiver span, active and final unique delivery, packet/body byte
counts, and the END/DRAIN count. Invalid payload, wrong stage/peer, sequence
overflow, premature abort, missing marker, serial panic, unexpected boot or
STA disconnect invalidates the capture. Duplicates, out-of-order arrivals,
send failures and tail packets are reported separately; they do not inflate
unique bytes. A paced candidate needs at least 99% active **and** final
delivery, actual TX rate at least 95% of target, and a DATA window at least
99% of requested duration. The unpaced result is an observation only.

The report's active UDP and body goodput use the larger of actual TX DATA
duration and the receiver's first-to-last active unique span. The highest
three-repeat passing target and its body goodput remain separate from the
unpaced saturation result. A passed upper search limit is labeled as a search
ceiling; no result is a PHY maximum or proof of long-term capacity.

Private `ap-raw.log` and `sta-raw.log` retain complete serial data. The
incremental `events.jsonl` retains host commands and stage timestamps even if
interrupted. `report.json` and `report.md` include every search stage,
confirmation, failure and binary SHA256; the optional `--public-log` copies
only allowlisted firmware fields. Both devices must acknowledge STOP and emit
session-end, radio-shutdown, radio-result PASS, and DONE markers before the
capture can pass.
