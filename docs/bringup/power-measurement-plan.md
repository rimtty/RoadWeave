# 消費電力の計測手順（Issue #7 / #8 の前提）

更新日: 2026-09-06

## 目的

[電力予算](../power-budget.md)の設計仮定（待受 0.45 W、RX 0.85〜1.30 W、PTT 0.70〜1.05 W、peak 2.5〜2.9 W）を
実測に置き換える。**平均だけでなく peak と rail ごと**に取る。

## 治具

| 方法 | 用途 | 精度 |
|---|---|---|
| USB 電力計（USB-C インライン、電流 mA 表示） | 待受/RX/TX の平均。最初の 1 回はこれで十分 | ±5% 程度、peak は見えない |
| INA226 breakout + 別 MCU（または XIAO 自身の I2C）で 1 kHz サンプリング | 平均と peak、TX burst の波形 | 1 mA 級、peak 可 |
| オシロ + 0.1 ohm シャント | 5 V FEM rail の TX peak、電源 droop（Rev.A 前に必須） | 波形 |

XIAO + WM6180 段階は USB 電力計と INA226 で足りる。HC01 V2 carrier と Rev.A では 3.3 V / 5 V rail を分けて測る。

## 状態の定義（各 60 秒以上、3 回）

| State | 条件 |
|---|---|
| S0 idle | HaLow 接続済み、受信のみ、音声なし、LCD なし |
| S1 RX speech | 相手が連続送信、こちらは受信・decode・再生（amp あり、音量中） |
| S2 PTT TX | こちらが連続送信（TX_ALWAYS）、speaker mute |
| S3 TX 1 MHz MCS0 | S2 を最長距離設定で（HaLow のみ） |
| S4 sleep 候補 | HaLow power-save / light sleep を有効化した idle（後回し可） |

各 state で: 平均電流 [mA]、peak 電流 [mA]（取れれば）、電圧 [V]、firmware commit、BW/MCS、TX 出力設定、codec/packet 設定、room temp。

## 記録テンプレート

```text
| Test | Date | HW | State | BW/MCS | TX dBm | Codec/packet | Vavg | Iavg mA | Ipeak mA | Pavg W | Note |
| bench-001 | 2026-09-xx | XIAO+WM6180 | S0 | 2M/auto | 21 | - | 5.05 | | | | |
```

`docs/power-budget.md` §7 の表と同じ列にして、そのまま転記できるようにする。

## 手順

1. USB 電力計を Mac と XIAO の間に挿す（データ線を通すタイプであること。給電専用だと console が使えない）。
2. smoke firmware で S0 相当（無線なし）を先に 1 回取り、ベースライン（ESP32 + USB Serial/JTAG のみ）を記録する。
3. HaLow firmware で S0〜S3 を順に取る。各 state の切替は console log に時刻付きで残す。
4. INA226 で取る場合は 1 kHz で 60 秒、CSV に落として平均・p99・max を出す。
5. 表を `docs/power-budget.md` §7 に追記し、§3 の仮定と差が 20% 以上あれば §4 の runtime 表を更新する。

## 注意

- USB 給電の 5 V が XIAO 上の 3.3 V LDO を通るので、XIAO 段階の値は「3.3 V 負荷 + LDO 損失」。Rev.A の buck 効率とは別。
- WM6180 の TX を測るときも RF 終端/減衰器は必須。空中送信しない。
- 音量を変えると amp の電流が大きく変わる。S1 は音量設定を明記する。
