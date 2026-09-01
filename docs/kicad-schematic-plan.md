# KiCad回路図ブロック案

更新日: 2026-09-02  
目標: Rev.A回路図を階層sheetへ落とす前のnet・責務分割

## 1. sheet構成

```text
00_top.kicad_sch
├── 01_power_usb_battery.kicad_sch
├── 02_esp32s3.kicad_sch
├── 03_halow_hc01.kicad_sch
├── 04_audio.kicad_sch
├── 05_display_ui.kicad_sch
├── 06_gnss.kicad_sch
├── 07_storage_expansion.kicad_sch
└── 08_connectors_test.kicad_sch
```

HC01 V2とMRF61_Aを同じsheetのvariant stuffingに押し込まない。`03_halow_hc01`を主schematicとし、MRF61_Aは別variant sheet/別board revisionで比較する。共通carrierが実証できた場合にだけ統合する。

## 2. 00_top

階層sheet間の信号と電源railだけを置く。

主要net:

```text
VBUS_5V, VBAT_RAW, SYS_BAT
SYS_3V3, HC01_3V3, HC01_5V0, AUDIO_5V0
HC01_SPI_{SCK,MOSI,MISO,CS_N,IRQ}, HC01_{RESET_N,WAKE,BUSY}
AUDIO_{BCLK,WS,MIC_DATA,SPK_DATA,AMP_SD_N}
AUX_SPI_{SCK,MOSI,MISO}, LCD_{CS_N,DC,BL_PWM}, SD_CS_N
GNSS_{RX,TX,PPS}, PTT_N, KEY_VOL_UP_N, KEY_VOL_DOWN_N
USB_D_P, USB_D_N
```

## 3. 01_power_usb_battery

含める回路:

- USB-C 5 V sink、CC resistors、ESD、fuse/polyfuse
- charger + power-path、1S LiPo connector、NTC
- battery protectorはprotected cell任せにせず二重化方針を明記
- 3.3 V buck、5 V boost、load switch
- fuel gaugeまたはbattery ADC option
- reverse current、USB挿抜、brownout対策
- railごとの0 ohm/current-shunt footprintとtest point

設計review:

- HC01 27 dBm TX load stepで5 Vが規定内
- 5 V boost起動中はAMP_SD_Nをlow、HC01 RESET_Nをlow
- USB給電中の充電発熱と同時TXのthermal worst case
- battery connector逆挿し防止、cell short対策

## 4. 02_esp32s3

- ESP32-S3-WROOM-1-N16R8
- EN RC/reset button、BOOT button
- native USB D+/D-、ESD、series resistorsはEspressif guideline準拠
- antenna keepout（PCB antenna版を使う場合）
- GPIO labelは[GPIO allocation](gpio-allocation.md)と一致させる
- spare test pads、factory programming pads

MCUの2.4 GHz antennaとHaLow RF/coax、5 V boost switching nodeを物理的に離す。

## 5. 03_halow_hc01

- HT-HC01 V2 footprint、SPI、IRQ、RESET_N、WAKE、BUSY
- VBAT/VDD_IO 3.3 V、VDD_FEM 5 Vのlocal decoupling
- JTAG/unused GPIO pulldown footprints
- SPI series damping resistor footprints、CS/RESET default state
- 50 ohm controlled-impedance traceを最短でU.FLへ
- π matching footprintはreference designに従い、勝手に値を固定しない
- module/antenna keepout、shield can option
- conducted test用U.FLまたはswitchable RF test connector

Heltecの[公式SPI reference design](https://resource.heltec.cn/download/HT-HC01_V2/Reference_design/HT-HC01_V2_SPI.PDF)とdatasheetをERC前に再確認する。

## 6. 04_audio

- I2S MEMS mic、clock/data、L/R select、local low-noise decoupling
- MAX98357A級amp、gain strap、SD/mute、output filter/EMI parts
- 4 ohm speaker connector、ESD、short protection
- micとspeakerのacoustic separation、speaker chamberはmechanical要件へ渡す
- optional headset/line-outはP0 DNP

firmwareだけに依存せず、ESP reset時もampをmuteするpulldownを置く。class-D traceをGNSS/RF/micから離す。

## 7. 05_display_ui

- ST7789系display FPC/connectorまたはmodule header
- AUX SPI SCK/MOSI、LCD_CS_N、LCD_DC
- backlight transistor + PWM、current limit
- PTT、VOL+、VOL-、power key
- night brightnessとglove operationをmechanical仕様に反映

## 8. 06_gnss

- UART GNSS module + PPS test pad
- antennaタイプ（chip/patch/U.FL）はDNP variant
- backup supply/supercap option
- ESDとantenna keepout
- class-D、boost、HaLow TXからのdesense測定point

## 9. 07_storage_expansion

- microSD socket、card detect、ESD、series resistors
- AUX SPI共用、独立SD_CS_N
- P0はDNP可能だがroutingとfootprintは残す
- 3.3 V peak currentとhot insertionを考慮
- 将来のrecording/PMTiles用。Rev.A機能のboot依存にしない

## 10. 08_connectors_test

- factory pogo: 5 V、3.3 V、GND、EN、BOOT、USB D+/D-またはUART
- rail current links
- audio loopback/test input、speaker test pads
- GNSS UART/PPS
- HC01 SPI/IRQ/RESET/WAKE/BUSY
- RF conducted test connector
- board ID/revision resistor

## 11. KiCad milestone

1. Symbols/footprints sourceとlicenseを記録
2. top + power + MCU + HC01のみでERC
3. reference designとのpin-by-pin checklist
4. audio/display/GNSS/storage追加
5. stack-up/impedanceをfabricatorに確認してPCB開始
6. schematic PDF、BOM、netlist、ERC logをdesign review tagへ保存
