# P0-A software validation record

更新日: 2026-09-02  
対象commit: push前のworking tree

個人情報、注文情報、SSID/passphrase、device serialはこの記録へ保存しない。

## macOS host baseline

| Item | Result |
|---|---|
| Host | Apple Silicon Mac / macOS 27 pre-release |
| ESP-IDF | `v5.4.4` |
| CMake | Homebrew `4.4.3` |
| Ninja | Homebrew `1.13.2` |
| Target | `esp32s3` |

Homebrewはpre-release macOSをsupport対象外として警告したが、RoadWeaveのconfigure、compile、link、ESP32-S3 image生成は完了した。OS正式release後またはtool更新後は同じcheckを再実行する。

## RoadWeave XIAO-only smoke firmware

`firmware/scripts/check-toolchain.sh`を通した後、repository外のclean build directoryで`idf.py build`を実行した。

| Check | Result |
|---|---|
| ESP-IDF version check | PASS |
| Homebrew CMake/Ninja detection | PASS |
| ESP32-S3 compile/link | PASS |
| `roadweave.bin` | `0x291b0` bytes |
| Smallest app partition free | 84% |
| Flash/USB/PSRAM実機診断 | hardware到着待ち |

このfirmwareはWM6180/MM6108を初期化せず、`HALOW_TX=DISABLED`を出力する。build成功は実機の8 MiB flash/PSRAMやUSB data pathを証明しない。

## Official Morse Micro porting assistant

`morsemicro/halow=2.11.2-esp32-2:porting_assistant`をcomponent registryから生成し、公式`seeed_xiao_esp32s3-seeed_xiao_mm6108` profileでコンパイルした。

| Check | Result |
|---|---|
| Exact component example generation | PASS |
| ESP32-S3 target configure | PASS |
| Official XIAO/MM6108 defaults load | PASS |
| BCF selection | `bcf_fgh100mhaamd.bin` |
| MM6108 firmware link | PASS |
| `porting_assistant.bin` | `0xa0680` bytes |
| Hardware porting tests | hardware/RF fixture待ち |

生成projectは`firmware/porting_assistant/`にあり、再生成物としてGit管理しない。コンパイルのみ実施し、flash、MM6108初期化、RF送信は実施していない。

## Remaining evidence

- Windows native PowerShellで`check-toolchain.ps1`とbuildを実行
- GitHub Actions Linux buildをmain push後に確認
- XIAO 3台でUSB/flash/PSRAM smokeを実行
- 安全なRF fixture確定後、WM6180 3組でporting assistantを実行
