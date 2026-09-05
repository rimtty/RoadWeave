# RoadWeave 実行スケジュール

更新日: 2026-09-05

到着日は物流で変動するため、日付だけでなくgate完了を次工程の開始条件にする。GitHub Project #2のStart date、Target date、Iterationをsource of truthとして同期する。

## 現在のP0-A

| 期間 | Task | Gate / 成果物 | GitHub |
|---|---|---|---|
| 2026-09-02〜09-03 | firmware/toolchain準備 | ESP-IDF 5.4.4固定、XIAO smoke build、初回起動手順 | 新規P0-A prep task |
| 2026-09-03〜09-08 | 到着・外観・XIAO単体試験 | 3台すべてUSB/flash/PSRAM PASS | Issue #1 |
| 2026-09-04〜09-11 | WM6180 stack / porting assistant | 3組すべて公式profileで全項目PASS | Issue #2 |
| 2026-09-10〜09-18 | AP/STA・UDP・復帰 | 2 node接続、8時間soak、再起動復帰 | Issue #3 |

物流が遅れた場合はIssue #1〜#3を同じ日数だけslideし、RF安全gateや3台全数確認を省略して取り戻さない。

## 2026-09-05時点の状況

| 項目 | 状態 |
|---|---|
| XIAO ESP32S3 | 1台到着・Gate 1 PASS（[log](bringup/logs/2026-09-05-p0a-gate1-node1.md)）。残り2台は未実施 |
| Wio-WM6180 x3 | 到着待ち |
| HC01P V2 + HT-HC01P HAT x3 | 手元にあり。位置付けは[ADR-0003](decisions/0003-hc01p-hat-breakout-and-linux-bridge.md)（Proposed） |
| SPH0645 x3 | 手元にあり。amp/speaker/PTTはIssue #18で到着待ち |
| RF fixture | **未発注**。Gate 3は50 ohm終端のみで可、Gate 4はattenuator chainが必要 |
| ESP-IDF v5.4.4 | `/tmp`配下の一時installだったため`~/esp/v5.4.4/esp-idf`へ移設中（[開発環境](development-environment.md)） |

Issue #2（target 09-11）はWM6180到着とRF fixture確保に依存するため、到着日確定後に同じ日数だけslideし、理由をIssue commentへ残す。

### 到着待ちの間に進める作業（hardware不要または手元部品で可能）

| # | 作業 | 状態（2026-09-05） |
|---|---|---|
| 1 | 残り2台のXIAO Gate 1（到着次第、各10分） | 待ち |
| 2 | RWP/0.1のserialize/parseとfloor state machineをhost unit test付きで実装（Issue #5前半） | **完了**: `firmware/components/rwp`、host test 176 checks PASS、CIにhost-tests job追加 |
| 3 | Opus encode/decodeのCPU/RAM benchmark（Issue #14前倒し） | **完了**: [結果](bringup/opus-bench-2026-09-05.md)。12 kbps/20 ms/c0で encode 18.7% + decode 4.3%（1 core比） |
| 4 | SPH0645のI2S capture確認（Issue #4前半） | **firmware完成・実配線待ち**: `firmware/experiments/audio_bench`。mic未接続でboot/I2S/meter動作を確認済み |
| 5 | I2S mic/amp loopbackとPTT hard mute（Issue #4後半） | **firmware完成・部品到着待ち**: 同上（loopback / test tone / PTT muteをKconfigで切替） |
| 6 | HT-HC01P HATの回路図から40 pin→SPI/BUSY/RESET_N/WAKE対応を確認し`gpio-allocation.md`へ追記 | 未着手（要: 回路図入手） |
| 7 | RF fixture発注: U.FL(MHF1)–SMA pigtail x3、SMA 50 ohm終端 x3、SMA 30 dB固定attenuator(2 W) x2、SMA F-F barrel x1 | 未発注 |

branch `feat/p0-prep-rwp-audio` に 2〜5 の成果がある。

## P0-B以降

| 期間 | Phase | 主な成果 |
|---|---|---|
| 2026-09-15〜09-23 | P0-B audio local | I2S mic/amp loopback、hard mute、underrun測定 |
| 2026-09-15〜09-25 | P0-B protocol | RWP/0.1 packet、PTT lease、malformed/sequence test |
| 2026-09-22〜10-02 | P0-B 2-node voice | IMA-ADPCM/UDP、100〜150 ms目標 |
| 2026-09-29〜10-09 | P0-B measurement | latency/loss/airtime/power測定 |
| 2026-10-01〜10-30 | EVT readiness | 電源rail実証、Rev.A KiCad review |
| 2026-10-26〜11-27 | P1 group voice | 4-node coordinator、targeted PTT、mute/gain、UI |
| 2026-12-01〜2027-01-29 | P2 convoy/Japan line | GPS convoy/breadcrumb、MRF61_A/FGH100M-J比較 |
| 2027-01-18〜03-19 | Future product spikes | Opus、recording、PMTiles-from-microSD |

## 次の判断点

1. **XIAO Gate:** 3台とも8 MiB flash/PSRAMを認識するか
2. **WM6180 Gate:** 公式profileでSPI、chip ID、FW/BCF、BUSYが通るか
3. **Network Gate:** 2台でAP/STAとUDPが8時間安定するか
4. **Product-line Gate:** HC01 V2 carrierへ進むか、FGH100M-JまたはMRF61_Aを優先するか
5. **P0-B Gate:** audio部品を購入し、IP-PTT latency測定へ進むか

## スケジュール変更ルール

- hardware到着前にsoftware prepとtest templateは完了させる
- blockerはIssueへlogと写真を添付し、次taskを`In progress`へ移さない
- 期日変更時は理由をIssue commentへ1行で残す
- 規制/安全gateは日程都合で省略しない
