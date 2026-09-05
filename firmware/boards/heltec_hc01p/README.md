# Heltec HC01P (V2) board files

取得日: 2026-09-05、出典: Heltec resource center。

| File | Source | SHA-256 |
|---|---|---|
| `bcf_HC0P.bin` (1,150 B, RISC-V ELF) | https://resource.heltec.cn/download/HT-HC01P/BCF/driver_1_15_3/bcf_HC0P.bin | `57c50cb2c1d51187667677b392684285c616ddbf3fe262c3aeece342f446773e` |
| `hc01p_pin_map.jpg` | https://resource.heltec.cn/download/HT-HC01P/Pin_Map/hc01p_pin_map.jpg | `2fbcd285b229137d56ad2b377bdc764d7ad93718d2e350b3c96c2cd4927ffbec` |
| `sdkconfig.defaults.seeed_xiao_esp32s3-heltec_hc01p_v2` | this repo (draft) | - |

BCF は Heltec 配布物であり license は未確認。再配布せず、必要なら上記 URL から再取得する。
Beyondlogic の記事では V1 用 `bcf_HC01P.bin` と V2 用 `bcf_HC01P-V2.bin` が別とされるが、resource center には
`bcf_HC0P.bin` 1 本しかない。**V2 用 BCF は Heltec に確認する**（Issue #2 / A-01）。

## HC01P mini PCIe pin map（pin_map.jpg より、host に必要な信号だけ）

| mini PCIe pin | Signal | Morse component Kconfig |
|---:|---|---|
| 1 | SPI_CS / SDIO_DATA3 | `CONFIG_MM_SPI_CS` |
| 2 | SPI_MOSI / SDIO_CMD | `CONFIG_MM_SPI_MOSI` |
| 3 | SPI_MISO / SDIO_DATA0 | `CONFIG_MM_SPI_MISO` |
| 4 | SPI_CLK / SDIO_CLK | `CONFIG_MM_SPI_SCK` |
| 48 | SDIO_DATA1 / SPI_INT | `CONFIG_MM_SPI_IRQ` |
| 10 | WAKEUP_IN | `CONFIG_MM_WAKE` |
| 11 | BUSY | `CONFIG_MM_BUSY` |
| 42 | RESET_N | `CONFIG_MM_RESET_N` |
| 6, 7, 27, 41, 52 | 3V3 | HAT 側で供給 |
| 5, 8, 9, 12, 13, 16, 19, 22, 28, 33, 36, 40, 44, 51 | GND | 共通 GND |

SPI mode に必要な 8 信号はすべて edge に出ている。UART/JTAG は出ていない。

## HT-HC01P HAT 40 pin header の対応（要実測）

Web 上の一次資料では 40 pin header の割り当てを確定できなかった（Heltec の Schematic_diagram は空、
「Morse Micro MMECH06 と同じ GPIO 割り当て」という記述のみ）。HAT が手元にあるので、**mini PCIe socket の
各 pin から 40 pin header へテスターの導通で当たる**のが最短で確実。

手順（無通電、module を抜いた状態）:

1. mini PCIe socket の pin 番号を確認する（奇数 pin は基板上面側、偶数 pin は下面側。pin 1 は切り欠きから遠い端）。
2. 上表の 8 信号 + 3V3 について、socket pin と 40 pin header の各 pin の導通を調べ、下表を埋める。
3. 期待値: SDIO 系は Raspberry Pi の SDIO alt 機能（BCM GPIO22=CLK, 23=CMD, 24=DAT0, 25=DAT1, 26=DAT2, 27=DAT3）
   = header pin 15, 16, 18, 22, 37, 13。RESET_N / WAKE / BUSY は別の GPIO。
4. 3V3 が header の 3V3（pin 1/17）直結か、HAT 上のレギュレータ経由（5V pin 2/4 から）かも確認する。

| Signal | mini PCIe pin | HAT header pin | RPi BCM GPIO | XIAO GPIO（profile） |
|---|---:|---:|---:|---:|
| SPI_CLK | 4 |  |  | 7 |
| SPI_MOSI | 2 |  |  | 9 |
| SPI_MISO | 3 |  |  | 8 |
| SPI_CS | 1 |  |  | 4 |
| SPI_INT | 48 |  |  | 3 |
| BUSY | 11 |  |  | 5 |
| RESET_N | 42 |  |  | 1 |
| WAKEUP_IN | 10 |  |  | 2 |
| 3V3 | 6/7/27/41/52 |  |  | - |

埋まったら `docs/gpio-allocation.md` の HC01 節へ転記する。

## 配線上の注意

- HaLow SPI は jumper wire なら 4 MHz 程度から始め、安定したら上げる（Morse community の実績: jumper 4 MHz、はんだ 40 MHz）。
- RESET_N は driver が能動的に driving する必要がある。pull-up 任せにしない。
- HC01P V2 は on-card で 5 V（FEM）を昇圧するため、HAT への 3V3 供給能力に余裕を持たせる。XIAO の 3V3 pin ではなく
  5 V → HAT のレギュレータ、または別電源から給電する。
- WM6180 と HC01P は同時に XIAO へ載せない（同じ GPIO を使う）。
