# ESP32-S3 GPIO割り当て案

更新日: 2026-09-05  
対象: ESP32-S3-WROOM-1-N16R8 + HC01 V2、Rev.A draft 0.1

## 1. 方針

- P0-AのXIAO ESP32S3 + Wio-WM6180では、このRev.A案を配線せず、Morse Micro公式board profileをそのまま使う。
- HC01 V2はESP-IDF向けMorse Micro componentに合わせ、SDIOではなくfull-duplex SPI + DMA + level interruptから開始する。
- GPIO26〜37はflash/Octal PSRAM競合を避け、使用しない。
- GPIO0/3/45/46はstrapping pinのため通常機能に割り当てない。
- GPIO19/20はnative USB D-/D+として確保する。
- JTAG既定pinをLCDに使う代わりに、debugはnative USB Serial/JTAGを基本とする。
- peripheral signalはGPIO matrixでroutingする。表の割り当ては回路図作成前にESP-IDF buildと実配線で検証する。

制約の根拠は[ESP32-S3 GPIO documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/gpio.html)と[ESP32-S3-WROOM-1 datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf)。

## 1.1 P0-A reference profile

Morse Micro HaLow component `2.11.2-esp32-2`同梱の`sdkconfig.defaults.seeed_xiao_esp32s3-seeed_xiao_mm6108`をsource of truthにする。

| Signal | GPIO |
|---|---:|
| RESET_N | 1 |
| WAKE | 2 |
| SPI IRQ | 3 |
| SPI CS | 4 |
| BUSY | 5 |
| SPI SCK | 7 |
| SPI MISO | 8 |
| SPI MOSI | 9 |

この割り当てはWio-WM6180/XIAO header用であり、下記HC01 V2 Rev.A案へ転記しない。BCFは`bcf_fgh100mhaamd.bin`、chipはMM6108を指定する。

## 2. 割り当て表

| GPIO | Net / function | Dir | Boot state / circuit note | Status |
|---:|---|---|---|---|
| 0 | BOOT | input | strapping、button to GND、通常機能禁止 | fixed |
| 1 | BAT_SENSE | analog in | divider + RC、ADC calibration要 | draft |
| 2 | CHG_STATUS | input | charger open-drain、pull-up | draft |
| 3 | reserved | - | strapping、test padのみ | fixed |
| 4 | PTT_N | input | active-low、external pull-up、debounce | draft |
| 5 | KEY_VOL_UP_N | input | active-low | draft |
| 6 | HC01_SPI_SCK | output | DMA SPI bus、series R footprint | draft |
| 7 | HC01_SPI_MOSI | output | HC01 pad 21 / SPI_MOSI | draft |
| 8 | HC01_SPI_MISO | input | HC01 pad 17 / SPI_MISO | draft |
| 9 | HC01_SPI_CS_N | output | HC01 pad 22 / SPI_CS | draft |
| 10 | HC01_SPI_IRQ | input | HC01 pad 16 / SPI_INT、level interrupt確認 | draft |
| 11 | HC01_RESET_N | output | HC01 pad 35、active-low | draft |
| 12 | HC01_WAKE | output | HC01 pad 36 | draft |
| 13 | HC01_BUSY | input | HC01 pad 34 / MM_GPIO_0 | draft |
| 14 | AUDIO_BCLK | output | mic/amp shared clock | draft |
| 15 | AUDIO_WS | output | 16 kHz mono、mic/amp shared | draft |
| 16 | MIC_DATA | input | I2S MEMS DOUT | draft |
| 17 | SPK_DATA | output | MAX98357A DIN | draft |
| 18 | AMP_SD_N | output | boot時mute、active-high enable | draft |
| 19 | USB_D_N | bidir | native USB | fixed |
| 20 | USB_D_P | bidir | native USB | fixed |
| 21 | KEY_VOL_DOWN_N | input | active-low | draft |
| 26〜37 | NC | - | module flash/PSRAM path回避 | fixed |
| 38 | AUX_SPI_SCK | output | LCD + future microSD shared bus | draft |
| 39 | AUX_SPI_MOSI | output | LCD + microSD | draft |
| 40 | AUX_SPI_MISO | input | microSD、LCDは未接続 | draft |
| 41 | LCD_CS_N | output | display chip select | draft |
| 42 | LCD_DC | output | display data/command | draft |
| 43 | GNSS_RX | input | UART1 RX。ROM UART0出力との実装影響を確認 | risk |
| 44 | GNSS_TX | output | UART1 TX。USB console使用、UART0 console無効化 | risk |
| 45 | reserved | - | strapping | fixed |
| 46 | reserved | - | strapping、input-only制約にも注意 | fixed |
| 47 | SD_CS_N | output | P0はDNP可、test pad | reserved |
| 48 | LCD_BL_PWM | output | external transistorでbacklight制御 | draft |

LCD resetはESP32 ENに直接結ばず、専用RC/supervisorまたはLCD power/reset回路で処理する。GNSS UARTの43/44はboot logとの競合をbenchで確認し、問題があればKEYまたはADC pinとの交換を行う。

## 2.1 HC01P V2 + HT-HC01P HAT（breadboard 段階）

mini PCIe 側の信号は [firmware/boards/heltec_hc01p/README.md](../firmware/boards/heltec_hc01p/README.md) で確定済み
（CS=1、MOSI=2、MISO=3、CLK=4、WAKE=10、BUSY=11、RESET_N=42、SPI_INT=48）。HAT の 40 pin header 側は実測で埋める。
XIAO 側は WM6180 公式 profile と同じ GPIO を使う draft profile `sdkconfig.defaults.seeed_xiao_esp32s3-heltec_hc01p_v2` を用意した。

## 3. HC01 V2 module pad対応

| HC01 pad | Name | ESP32 GPIO / treatment |
|---:|---|---|
| 16 | MM_SDIO_D1 / SPI_INT | GPIO10 |
| 17 | MM_SDIO_D0 / SPI_MISO | GPIO8 |
| 18 | MM_SDIO_CLK / SPI_SCK | GPIO6 |
| 19 | MM_VDD_IO | 3V3、hostと同一rail |
| 21 | MM_SDIO_CMD / SPI_MOSI | GPIO7 |
| 22 | MM_SDIO_D3 / SPI_CS | GPIO9 |
| 25 | VBAT | HC01_3V3 |
| 32 | VDD_FEM | HC01_5V0 |
| 34 | MM_GPIO_0 / BUSY | GPIO13 |
| 35 | RESET_N | GPIO11 |
| 36 | WAKE | GPIO12 |
| 38 | ANT_1 | 50 ohm -> U.FL/SMA path |
| JTAG, unused GPIO | unused | datasheetに従い10 k pulldown、DNP optionを含めreference design確認 |

HC01 V2 pad定義と未使用pin処理は[公式datasheet](https://resource.heltec.cn/download/HT-HC01_V2/Datasheet/HT-HC01_V2.pdf)を回路図source of truthにする。

## 4. firmware config対応

Morse Micro componentで必要になる代表値:

```text
CONFIG_MM_SPI_SCK=6
CONFIG_MM_SPI_MOSI=7
CONFIG_MM_SPI_MISO=8
CONFIG_MM_SPI_CS=9
CONFIG_MM_SPI_IRQ=10
CONFIG_MM_RESET_N=11
CONFIG_MM_WAKE=12
CONFIG_MM_BUSY=13
CONFIG_MM_CHIP_MM6108=y
```

実際のKconfig symbol名とcomponent versionは導入時に固定し、BCF/FW versionも同じcommitに記録する。現行component情報は[Morse Micro HaLow for ESP-IDF](https://components.espressif.com/components/morsemicro/halow)を参照。

## 5. 回路図release前チェック

- [ ] N16R8 module variantの実pin availabilityをsymbol/footprintと照合
- [ ] PTT、key、ampがreset中に安全stateになる
- [ ] HC01 IRQが選択GPIOでlevel-triggered interruptとして動く
- [ ] HC01 SPIがfull-duplex DMAでporting assistantを通る
- [ ] USB download/consoleがLCD/GNSSなしでも使える
- [ ] LCD + microSD shared SPIでCSが同時assertされない
- [ ] ADC dividerがsleep currentと測定精度の両方を満たす
- [ ] strapping pinに外部回路が誤電圧を与えない
