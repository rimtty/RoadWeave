# Firmware

ESP-IDF application root。最初のapplicationはSeeed Studio XIAO ESP32S3単体のUSB console、flash、PSRAMを確認する。WM6180/MM6108は初期化せず、HaLow送信を行わない。

## 固定version

- ESP-IDF: `v5.4.4`
- Morse Micro HaLow component: `2.11.2-esp32-2`（pre-release）
- 公式board profile: `sdkconfig.defaults.seeed_xiao_esp32s3-seeed_xiao_mm6108`

正確な値は[`toolchain.lock`](toolchain.lock)をsource of truthとする。Morse Micro componentはESP-IDF `>=5.4.4,<6.0`を要求するため、P0-A中は別versionへ暗黙に更新しない。

## XIAO単体smoke test

ESP-IDFをinstallして環境をexportした後:

```bash
cd firmware
./scripts/check-toolchain.sh
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/cu.usbmodemXXXX flash monitor
```

macOSではport名を次で確認できる。

```bash
ls /dev/cu.usbmodem*
```

成功時はboot logに`P0A_XIAO_SMOKE=PASS`と`HALOW_TX=DISABLED`が表示される。期待値は8 MiB flashと8 MiB Octal PSRAM。USB-Cは電源、書き込み、USB Serial/JTAG consoleを兼ねる。

## WM6180 porting assistant

[`scripts/create-porting-assistant.sh`](scripts/create-porting-assistant.sh)は、固定versionの公式exampleを`firmware/porting_assistant/`へ生成する。このdirectoryは再生成物のためGit管理しない。

このexampleはMM6108 firmware/BCFをloadし、bus/chip/throughput/BUSYを試験する。**最初のXIAO単体smoke testでは実行しない。** RF端子を開放したまま通電・送信せず、[初回起動手順](../docs/bringup/xiao-wm6180-first-boot.md)のgateを満たしてから使う。

## 方針

- reference bring-upはXIAO ESP32S3 + Wio-WM6180を公式profileで行う
- HC01 V2は製品主開発ライン、FGH100M-JとMRF61_Aは日本適合候補として別gateを持つ
- secret、SSID passphrase、recording data、注文情報をrepositoryへcommitしない
- `dependencies.lock`は導入後にcommitし、managed component自体はcommitしない

## 想定component境界

```text
components/
  halow_port/
  audio_pipeline/
  voice_transport/
  group_service/
  position_service/
  route_model/
  ui/
  storage/
```

P0-AではXIAO単体smoke、公式`porting_assistant`、`sta_connect`、`softap`の順に独立して通す。applicationへの統合はその後。
