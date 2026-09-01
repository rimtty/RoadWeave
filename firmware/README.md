# Firmware

ESP-IDF application root。現時点は依存なしの最小scaffoldで、HC01 V2を起動しない。

## 方針

- ESP-IDF versionとMorse Micro component versionをPoC開始時に固定する
- `morsemicro/halow`はpre-releaseのため、latest追従にしない
- HC01 V2のBCF/FW整合をporting assistantで確認してからdependencyを追加する
- secret、SSID passphrase、recording dataをrepositoryへcommitしない

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

P0-Aではまず公式componentの`porting_assistant`、`sta_connect`、`softap`に相当する検証を独立して通す。applicationへの統合はその後。

