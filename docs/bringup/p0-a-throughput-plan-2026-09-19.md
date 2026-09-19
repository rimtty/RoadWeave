# HaLow最大UDP転送速度の実証計画 — 2026-09-19

目的は、低負荷のstop-and-wait echoから進み、現在のESP32-S3＋SPI＋Wio-WM6180経路で受信できる最大転送速度を実測すること。計画・管理・独立レビューはAstra、実装はSol、実機操作は主担当が行う。

既存の[有限通信実証](p0-a-validation-report-2026-09-19.md)を保存したmain `d6bb978496d232f0bc813e0e0c7d5486a1a29215`から機能別branchを作る。長時間運転、電源断反復、測定器の必要な試験は今回も対象外。

## 測る値

1. **最高合格負荷:** 受信率99%以上、送信実績が目標の95%以上、60秒の反復3回すべてで成立した最高の検査済み送信負荷。受信側unique bytesによるgoodputの各回・範囲も示す。
2. **飽和時の最大観測受信速度:** 無制限または合格上限を超える送信負荷で観測した最大receiver goodput。損失・送信失敗を併記し、上記の合格負荷と区別する。

検査点の間隔と有限時間に制約される実測上限であり、PHYの理論最大速度や長時間持続能力とは呼ばない。速度の単位はbit/s（十進）とし、1200 BのUDP payload全体と、試験headerを除くapplication bodyを区別する。UDP/IP/MACのheader、再送、ackはpayload goodputに含めない。

## 条件と比較

| 条件 | 固定する内容 |
|---|---|
| 機器 | COM4 AP、COM5 STA1。COM6は無線停止状態を維持 |
| 通信経路 | ESP32-S3→SPI→MM6108→HaLowのみ。内蔵Wi-Fi不使用 |
| SPI／console | SDK設定40 MHz。actual clockはログで確認できた場合に別記。consoleはnative USB Serial/JTAG |
| RF | 既確認の使用場所、装着済みアンテナ、設定上限1 dBmを継続 |
| 幅と周波数 | 1 MHz/ch3/opclass1/903.5 MHz、2 MHz/ch6/opclass2/905 MHz |
| 方向 | STA→APとAP→STAを別々に測る。同時双方向にしない |
| IP | 既存の検証済みDHCP、leaseとUDP bindを確認してから測定 |
| UDP | 1200 B payload、echo待ちなしの片方向stream。packet単位のUART出力なし |
| 時間 | 初期warmup5秒、探索30秒、確認60秒×3。各stage終了後3秒以上drain。方向ごとにsessionを分け、必要ならglobal watchdogを1800秒へ設定 |
| 記録 | source/binary SHA256、実測duration、rate、全counter、起動・lease・正常停止を保存 |

中心周波数も変わり、周囲雑音・物理配置を実験室で固定できていないため、差を帯域幅だけの効果としない。受信goodputはCPU・lwIP・SPI・無線・受信検証処理を含む今回の経路の性能である。

## 実装契約

既存AP/STA初期化、DHCP adapter、起動IRQ対策、正常無線停止を再利用し、独立したthroughput modeを追加する。従来のecho modeを維持する。hostから受信側をARMし、確認後に送信側をSTARTする。stage IDは毎回変え、別stageや旧packetを加算しない。両端の最終counterは無線に依存しないUSB serialで取得する。

- UDP headerは16 B、magic、version、stage ID、sequence等を持ち、残り1184 Bを決定的なbodyとして検査する。byte orderはFWで明示し、hostのsummaryにheader bytesを記録する。最大262144 sequenceのbitmapは32 KiBで、上限超過は無効。
- sequenceは送信成功したdatagramに対して連番とする。attempt、send error、予定負荷からのskippedは別counter。send成功だけで受信成功を判断しない。
- 受信側はstageごとのbounded bitmap等でunique sequenceを判定し、duplicate、out-of-order、invalid、範囲外を別記する。最大sequenceから受信数を推定しない。overflowはstage無効とする。
- rate制御は単調時計を用い、処理遅延後に無制限のcatch-up burstを出さない。目標rateと実際の送信bytes/timeをともに出力する。rate0などで有限時間の飽和送信も可能にする。
- 1秒程度の集計sampleと最終summaryのみを出し、packet単位printfやUSBの転送速度を測定の律速にしない。throughput設定だけSDKログをINFOからWARNへ下げ、その設定を記録する。終端marker欠落やconsole write timeoutを成功としない。
- TX終了後もRXを3秒以上維持してqueueをdrainする。late/tail、receiverの最初・最後の受信時刻とspanを記録し、前stageの残留を次へ混ぜない。
- packet lossはhostで `(TX成功数−RX unique数)/TX成功数` として計算する。TX失敗率も別記。RX uniqueがTX成功数を超えた場合は集計不整合で無効。
- 主goodputの分母は、実際のTX測定時間とRX first→last spanの大きい方とする。短いfirst→last窓による過大評価を避け、drainで長く残ったbacklogの影響を隠さない。1秒sampleとtailを併読し、一瞬のpeakを持続値にしない。
- warmupは別stage、または明示phaseで分離する。省略・混入を避け、送受信counterの同じ測定範囲を保証する。
- bounded stage・idle watchdog・host STOPで終了可能にする。終了時は両端のSTOP ACK、radio shutdown、DONEまで確認する。

実装前に合意した制御は `RW_TPUT_CMD ARM stage=N duration_s=...`、`SEND stage=N peer=... duration_s=... rate_kbps=... payload_bytes=1200`、`DRAIN stage=N sent=N` とし、telemetryを `RW_TPUT_*` に分離する。RX_READY後にsenderがSTART制御packetを送り、ACKを得てからdata測定窓を開始する。ENDには成功送信総数を持たせ、ACKを最大3秒再試行する。host DRAINはEND喪失時もsummaryを回収するが、ENDなしは有効測定にしない。

END前のactive uniqueを主goodputへ使い、END後の新規受信はtailとして分ける。最終deliveryは両方を含めるが、最高合格負荷にはactive deliveryも99%以上を要求する。大量のqueueをdrainで受け切っただけの状態を持続合格にしない。DATA窓が指定時間の99%未満の場合やabort時は無効。送信処理の実際の延長時間も記録する。

既存SDKには `morsemicro__halow/examples/iperf` のUDP/TCP REPL例があり、`espressif/iperf-cmd ~0.1.3`へ依存する。ただしSTA接続用の独立例で、今回のAP/DHCP/board対策を含む完成済みbenchの代替ではない。MorseのUDP server実装も受信時刻・out-of-sequenceを扱う参考になるが、そのfirst/last durationだけで今回の合格値を定義しない。TCPは接続経路と正確な受信計数を低コストで再利用できる場合の追加項目とし、UDP実測を先に完了する。

## 実行順と判定

各幅・方向の4条件で同じ手順を用いる。

1. 初期化・DHCPを確認し、warmup5秒を行う。最初に低負荷でpacketの計数一致を確認する。
2. 30秒stageで128、256、512 kbit/s、1、2、4、8 Mbit/sと増やす。99%受信または目標rateの95%追随を満たさない最初の点を得たら、低損失境界の探索を止める。
3. 最後の合格点と最初の不合格点の間を2段階の中間rateで絞る。最低rateから失敗した場合は低rateへ下げ、上限8 Mbit/sまで合格した場合は無制限stageの実績から追加rateを決める。
4. 境界候補を60秒×3回測定する。3回すべてactive/final受信率99%以上・送信追随95%以上、予期しないreset、lease消失、invalid、計数不整合、buffer overflowなしを合格条件とする。1回でも満たさなければ候補を下げて確認し、変動も残す。
5. 別の有限飽和stageを1回実施する。過負荷を承知で測る観測点としてlossを報告し、探索や反復の失敗を隠すためには使わない。
6. 両端を正常停止してから方向／幅を変える。すべて終わったら全3台の現在状態と、再起動した時に開始するFWの挙動を記録する。

送信側が目標を生成できない場合は「RF受信上限」と断定せず、今回の送信path上限として実際のrateを報告する。上限まで不合格点が得られなければ「少なくともこの実測値」であり、最大を確定したとはしない。

## Branchと保存

| Branch | 担当・内容 | 確認 |
|---|---|---|
| `codex/halow-throughput-plan` | Astra: 計画・計測定義 | 実行前に条件・判定を固定 |
| throughput firmware feature | Sol: 有限stream送受信・counter・制御 | 両role/両幅build、既存mode維持、bounded終了 |
| throughput host tools feature | Sol: ARM/START/drain・集計・探索 | 正常／loss／duplicate／epoch混入／rate未達／途中終了のoffline検査 |
| throughput results | Astra: 実測表・安全に抽出したログ・残件 | binary対応、raw hash、最高合格値と飽和値、全失敗点を保持 |

実機に合わせた実装schema・上限・探索点の調整は実行前に記録する。結果は幅×方向、送信目標と実績、RX payload/body goodput、loss、duration、反復範囲を一覧にし、秘密設定を含めない。過去の約82 kbit/s固定負荷の結果を最大速度として転用しない。
