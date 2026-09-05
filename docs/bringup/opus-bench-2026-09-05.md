# Opus benchmark on XIAO ESP32S3（Issue #14 前倒し）

実施日: 2026-09-05  
条件: libopus 1.5.2 fixed-point、`-O2`、ESP32-S3 @ 240 MHz、単一タスク、16 kHz mono、VOIP application、VBR、FEC/DTX off、
codec state は内部 RAM（`heap_caps_malloc(MALLOC_CAP_INTERNAL)`）。入力は合成音声（2 formant + 4 Hz AM + noise）3 秒。
firmware: `firmware/experiments/opus_bench`。

| kbps | frame | complexity | encode avg（1 core比） | enc max | decode avg（1 stream） | dec max | 合計（1 enc + 1 dec） | 実効 bitrate |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 8 | 20 ms | 0 | 3.72 ms（18.6%） | 4.06 ms | 0.84 ms（4.2%） | 0.90 ms | 22.8% | 8.3 |
| 12 | 20 ms | 0 | 3.73 ms（18.7%） | 4.08 ms | 0.86 ms（4.3%） | 0.94 ms | 23.0% | 12.3 |
| 16 | 20 ms | 0 | 5.06 ms（25.3%） | 5.55 ms | 1.06 ms（5.3%） | 1.18 ms | 30.6% | 16.3 |
| 8 | 20 ms | 3 | 5.03 ms（25.1%） | 5.45 ms | 0.85 ms（4.2%） | 0.91 ms | 29.4% | 8.1 |
| 12 | 20 ms | 3 | 7.44 ms（37.2%） | 8.07 ms | 1.05 ms（5.2%） | 1.18 ms | 42.4% | 12.1 |
| 16 | 20 ms | 3 | 7.47 ms（37.3%） | 8.07 ms | 1.07 ms（5.3%） | 1.20 ms | 42.7% | 16.1 |
| 12 | 20 ms | 5 | 10.42 ms（52.1%） | 10.98 ms | 1.06 ms（5.3%） | 1.19 ms | 57.4% | 12.4 |
| 12 | 40 ms | 0 | 7.08 ms（17.7%） | 7.76 ms | 1.26 ms（3.1%） | 1.34 ms | 20.9% | 12.2 |
| 12 | 40 ms | 3 | 14.48 ms（36.2%） | 15.55 ms | 1.64 ms（4.1%） | 1.82 ms | 40.3% | 12.1 |
| 24 | 20 ms | 3 | 7.51 ms（37.6%） | 8.11 ms | 1.10 ms（5.5%） | 1.23 ms | 43.1% | 24.2 |

2 stream 同時 decode（voice room 想定、12 kbps / 20 ms / c3）: encode 37.2% + decode 4.4% × 2 = **45.9%**。

メモリ: encoder state 24,548 B、decoder state 17,800 B。1 enc + 1 dec で内部 heap 42.5 KiB、1 enc + 2 dec で 60.4 KiB。
実行後の heap 減少なし（leak なし）。

## 追加計測（2026-09-06）: 1 encoder + N decoders（voice room 想定、12 kbps / 20 ms）

| complexity | decoders | encode | decode avg / stream | 合計（1 core 比） | 内部 heap |
|---:|---:|---:|---:|---:|---:|
| 0 | 2 | 18.6% | 3.5% | 25.7% | 60 KB |
| 0 | 4 | 18.6% | 3.1% | 31.2% | 96 KB |
| 0 | 6 | 18.6% | 3.0% | **36.6%** | 132 KB |
| 3 | 2 | 37.1% | 4.4% | 45.9% | 60 KB |
| 3 | 4 | 37.1% | 4.0% | 53.0% | 96 KB |
| 3 | 6 | 37.1% | 3.8% | **60.1%** | 132 KB |

decoder は台数が増えても 1 stream あたり 3〜4% で頭打ち（キャッシュが温まる）。6 stream 同時デコードでも codec 合計は
c0 で 37%、c3 で 60%（core 0 の 1 core 比）。

## 読み方

- CPU% は「1 core の 20 ms のうち何 % を使うか」。ESP32-S3 は 2 core なので、audio/network を core 0、UI を core 1 に分ければ
  complexity 3 でも encode + 2 decode が core 0 の半分以下に収まる。
- **complexity 0 と 3 の差が大きい**（12 kbps で 18.7% → 37.2%）。音質差は実聴で判断し、P0-B では c0〜c2 を既定候補にする。
- 40 ms frame は 20 ms と CPU% がほぼ同じで、packet 数が半分になる。airtime/Duty（Issue #10）には有利、遅延には不利。
- PSRAM に state を置いた場合も同じ値だった（別実行で確認）。state サイズが小さいためキャッシュに乗る。
- 前提の違い: 実機では I2S、HaLow SPI、UDP の割り込みが加わる。P0-B 実装後に同じ表を取り直す。

## 結論（Issue #14 / #10 向け）

- Opus 12 kbps / 20 ms / complexity 0〜2 は、P0-B の IMA-ADPCM から置き換えても CPU 余力がある。
- airtime 見積りには「12 kbps + RWP header 36 B + UDP/IP 28 B」で 1 packet 約 94 B / 20 ms（約 37.6 kbps wire）を使う。
  40 ms なら約 64 B/packet 相当で約 25 kbps wire。
