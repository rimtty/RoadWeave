# 電力・バッテリー予算

更新日: 2026-09-02

## 1. 基準

- Battery: protected 1S LiPo、3000 mAh、3.7 V nominal = 11.1 Wh
- 設計上の利用可能エネルギー: nominalの85% = 9.44 Wh
- HC01 V2通常mode: 2 MHzを第一候補
- Long-range: 1 MHz MCS0/MCS10を評価
- LCD: 操作/RX時100%、待受20%、休憩時10%または消灯
- ESP32-S3内蔵2.4 GHz Wi-Fiは通常OFF、BLEは将来のremote PTT時のみ

85%係数にはconverter損失、低電圧cutoff、温度・経年の余裕をまとめて含める。セルの安全設計やfuel gauge calibrationの代用ではない。

## 2. HC01 V2のdatasheet基準値

27 dBm、MCS0、100% duty時:

| Bandwidth | 3.3 V current | 5 V FEM current | 電力概算 |
|---|---:|---:|---:|
| 1 MHz | 45 mA typ | 390 mA typ | 2.10 W |
| 2 MHz | 46 mA typ | 340 mA typ | 1.85 W |
| 4 MHz | 47 mA typ | 295 mA typ | 1.63 W |
| 8 MHz | 51 mA typ | 260 mA typ | 1.47 W |

1 MHz listenは3.3 V側26 mA typに加えてLNA時5 V側17 mA。snooze 27 uA typ、deep sleep 1 uA typ、hibernate 0.05 uA typとされる。値と条件は[HT-HC01 V2 datasheet Rev.2.0](https://resource.heltec.cn/download/HT-HC01_V2/Datasheet/HT-HC01_V2.pdf)を参照。

帯域を広げれば送信時間を短縮できるが、link budgetが下がる。2 MHzを採用する理由は「常に最も省電力」だからではなく、音声burstの送信時間と2 dBの感度差を実車で比較する価値があるため。

## 3. システム電力の初期予算

| ブロック | 待受 | RX speech | PTT平均 | 瞬間peak | 備考 |
|---|---:|---:|---:|---:|---|
| HC01 V2 | 0.17 W | 0.18〜0.22 W | 0.35〜0.65 W | 1.85〜2.10 W | PTT平均は10〜25% airtime仮定。要実測 |
| ESP32-S3 | 0.10 W | 0.15 W | 0.18 W | 0.25 W | PSRAM、CPU負荷で変動 |
| LCD/backlight | 0.04 W | 0.10 W | 0.10 W | 0.16 W | PWM必須 |
| GNSS | 0.07 W | 0.07 W | 0.07 W | 0.10 W | module未確定 |
| Mic | 0 W相当 | 0 W相当 | 0.001 W | 0.001 W | I2S MEMS |
| Amp/speaker | mute | 0.25〜0.70 W | mute | 1.0 W超 | 平均音声出力依存 |
| Regulators/other | 0.07 W | 0.10 W | 0.12 W | 0.20 W | converter loss含む |
| **System** | **約0.45 W** | **約0.85〜1.30 W** | **約0.70〜1.05 W** | **約2.5〜2.9 W** | 設計仮定 |

PTT中にspeakerをmuteするため、最大RF TXと最大speaker出力は通常同時に発生しない。故障・起動・UI音など例外状態では同時負荷を禁止するfirmware interlockを入れる。

## 4. 利用profileとruntime

通常ツーリングprofileの一例:

| 状態 | 時間比 | 状態電力 | 加重電力 |
|---|---:|---:|---:|
| idle/listen、LCD dim | 65% | 0.45 W | 0.293 W |
| RX speech | 20% | 0.95 W | 0.190 W |
| PTT speech | 10% | 0.80 W | 0.080 W |
| UI/GPS操作 | 5% | 0.65 W | 0.033 W |
| **平均** | 100% |  | **0.60 W** |

`9.44 Wh / 0.60 W = 15.7 h`

感度表:

| 実平均電力 | 2000 mAh（usable 6.29 Wh） | 3000 mAh（usable 9.44 Wh） | 5000 mAh（usable 15.73 Wh） |
|---|---:|---:|---:|
| 0.45 W | 14.0 h | 21.0 h | 35.0 h |
| 0.60 W | 10.5 h | 15.7 h | 26.2 h |
| 0.75 W | 8.4 h | 12.6 h | 21.0 h |
| 1.00 W | 6.3 h | 9.4 h | 15.7 h |

製品目標は3000 mAhで **typical 15 h、worst practical 12 h**。datasheetから保証される値ではなく、P0/P1で更新する。

## 5. 電源rail要件

```text
USB-C 5 V
   |
charger + power path ---- protected 1S LiPo
   |
   +-- 3.3 V buck, >=1.0 A ---- ESP32 / HC01 VBAT+VDDIO / LCD / GNSS
   |
   +-- 5.0 V boost, >=2.0 A peak ---- HC01 VDD_FEM / audio amp
```

- Cell/BMS: 2 A continuous、3 A transient以上を候補条件にする
- 5 V boost: HC01 FEM 400 mA max級 + audio transientを考え、2 A peak級。soft-startとload transientを確認
- 3.3 V: ESP32 transientとHC01を合わせ1 A以上、rail dropをoscilloscopeで確認
- 各railに電流測定jumperまたは0 ohm linkとtest pointを置く
- `HC01_3V3`、`HC01_5V0`、`MCU_3V3`、`AUDIO_5V0`、`LCD_3V3`を少なくとも分離測定する
- battery温度監視、NTC、過充電/過放電/短絡保護を必須要件にする

## 6. 省電力state

| State | HaLow | LCD | GNSS | Wake source |
|---|---|---|---|---|
| Ride | listen/power-save tuned | 20%、RX/PTTで昇光 | 1 Hz | PTT、network、key |
| Pause | snooze/TWT候補 | 10%/off | low-power | key、timer |
| Shipping | hibernate/power cut | off | off | power key/USB |

Ride中に深いsleepへ入り、先頭のPTT冒頭を欠落させないこと。省電力は音声受信latencyを測りながら導入する。

## 7. P0測定表

測定値は次の形式で残す。

| Test | BW/MCS | TX dBm | Codec/packet | RF airtime | 3.3 V avg/peak | 5 V avg/peak | end-to-end latency | loss |
|---|---|---:|---|---:|---:|---:|---:|---:|
| bench-001 | TBD | TBD | ADPCM/20 ms | TBD | TBD | TBD | TBD | TBD |

電池時間を「PAの最大電力」または「codec bitrate」だけから推定しない。実packetのairtime、retransmission、speaker平均出力を測る。

## 8. v2 影響（2.8 インチ表示、6 stream、2026-09-06）

| 変更 | 待受 | RX speech | PTT | 根拠 |
|---|---:|---:|---:|---|
| 2.8 in バックライト（4〜6 LED、20 mA、boost 効率 85%）| +0.02 W（20%）| +0.10 W（60%）| +0.10 W | 1.47 in 比 |
| 6 stream decode（Opus c0）| 0 | +0.03 W | 0 | codec 37% でも CPU 電力の増分は小さい |
| **システム（v2）** | **約 0.47 W** | **約 0.98〜1.43 W** | **約 0.80〜1.15 W** | |

通常ツーリング profile（§4 と同じ配分）: 平均 **約 0.68 W** → 3,000 mAh（usable 9.44 Wh）で **13.9 h**、2,500 mAh では 11.6 h。
最低目標 12 h を守るため v2 は 3,000 mAh を採用する（モックの 2,500 mAh は不採用）。

## 9. MRF61_A日本ライン

MRF61_Aは3.3 V単一電源・最大13 dBmの別RF構成であり、HC01 V2の5 V FEM電力表を流用しない。低い送信出力から平均電力の低下は期待できるが、公開資料だけからsystem runtimeを確定しない。

日本ラインのEVK/試作carrierで、`idle / RX / 1 MHz MCS0 TX / audio burst / sleep`を同じ測定治具と利用profileで取り直す。MRF61_A版のbattery表はその実測後に追加し、HC01版との比較にはRF出力、antenna、channel、packet delivery条件を併記する。
