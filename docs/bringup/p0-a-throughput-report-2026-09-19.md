# HaLow UDP最大転送速度の実証 — 2026-09-19

**最大観測はUDP payload 3.115 Mbit/s、試験headerを除くbody 3.073 Mbit/s。** 2 MHz・STA→APで得た。受信率99%以上・送信追随95%以上を60秒×3回で満たした最高目標は2.5 Mbit/sで、その実受信はUDP 2.411–2.417 Mbit/sだった。[事前計画](p0-a-throughput-plan-2026-09-19.md)に従い、現在のESP32-S3→SPI→MM6108経路の1/2 MHz・上下4条件と、理由を記録した追加探索1件を完了した。

## 計測条件

COM4をAP、COM5をSTAとして用い、COM6は無線停止のままにする。アンテナと既確認の使用場所を継続し、内蔵Wi-Fiを使わない。SPIはSDK設定40 MHz、consoleはnative USB Serial/JTAG。SDKのpacket単位INFOを抑え、throughput設定のみWARNへ変更した。

UDP payloadは1200 Bで、16 Bの試験headerと1184 Bの検証bodyからなる。echo待ちはなく、一方向streamを受信側でunique sequenceとbody検証により数える。primary goodputはEND前のactive unique受信bytesを、TX実測DATA時間とRX active spanの大きい方で割る。END後のtailを別記し、drainで受け切っただけの状態を持続合格へ変更しない。

最高合格候補はactive/final受信率99%以上、目標rateへの送信追随95%以上を60秒×3回で満たした点とする。飽和時の最大観測受信値は別に扱う。1 MHzは903.5 MHz、2 MHzは905 MHzで、中心周波数と周囲雑音も変わるため差を幅だけの効果と断定しない。

## 結果

| 幅 | 方向 | 最高合格目標rate | 60秒×3 RX UDP/body goodput | active/final loss | 最大観測RX UDP/body | 判定 |
|---|---|---|---|---|---|---|
| 1 MHz | STA→AP | 1.250 Mbit/s | 1.216–1.242 / 1.200–1.225 Mbit/s | 0–0.0263% / 同左 | 1.369 / 1.350 Mbit/s | PASS |
| 1 MHz | AP→STA | 1.000 Mbit/s（追加探索） | 0.99469–0.99484 / 0.98142–0.98158 Mbit/s | 0% / 同左 | 1.195 / 1.179 Mbit/s（初回飽和） | PASS |
| 2 MHz | STA→AP | 2.500 Mbit/s | 2.411–2.417 / 2.379–2.385 Mbit/s | 0.0132–0.0928% / 同左 | 3.115 / 3.073 Mbit/s | PASS |
| 2 MHz | AP→STA | 2.000 Mbit/s | 1.941–1.951 / 1.915–1.925 Mbit/s | 0–0.2137% / 同左 | 2.631 / 2.596 Mbit/s | PASS |

最大観測値は同一最終sourceの有効な本測定stageから選び、warmupは除外する。1 MHz上りは2 Mbit/s指定時にTX実績が約1.374 Mbit/sへ頭打ちとなり、4298/4298受信・body goodput約1.350 Mbit/sだった。送信追随が95%未満なので、その指定rateは合格候補にしない。無制限送信stageは4243/4243、RX UDP/body約1.349/1.331 Mbit/s。

同条件の60秒3反復では7601/7603、7760/7760、7716/7716を受信し、2欠落を保持した。1.250 Mbit/sは探索点のうち3反復を満たした最高目標rateで、絶対的な上限値ではない。今回の上限は受信損失より送信pathの追随不足で先に制約された。

1 MHz下りでは最初の128 kbit/s探索が392/400（2%損失）となり、規定どおり低負荷側へ探索を進めた。112 kbit/sの60秒3回は各700/700で、UDP約111999 bit/s、body約110506 bit/sだった。

飽和stageの結果を見る前に、追加条件を固定した。有効な飽和stageがRX 512 kbit/s以上かつactive/final受信率99%以上なら、低負荷での失敗と高負荷での成功が両立し、損失が負荷に対して単調に増える前提では探索できないため、512・1000・2000・4000・8000 kbit/sを開始点とする追加探索を1ケースだけ行う。30秒探索・2段階絞込み・60秒3反復の基準は同じとし、元の損失と結果を残す。追加試験の失敗を理由に再追加はしない。この条件を満たさなければ2 MHzの予定試験へ進む。

実際の飽和stageは3759/3759、RX UDP/body 1.195422/1.179483 Mbit/sで条件を満たしたため、追加探索を1回実施した。1 Mbit/sの60秒3回は6219/6219・6217/6217・6218/6218で、body 0.981424–0.981580 Mbit/s。1.25 Mbit/s指定では送信実績1.153575 Mbit/s（92.29%）となり、追随条件を満たさなかった。追加caseの無制限送信は3626/3626、UDP/body 1.151408/1.136056 Mbit/sだった。

元の112 kbit/sを機器の最大速度とは扱わず、初期の損失変動で探索が低負荷側へ限定された結果として残す。初回の128 kbit/s・2%損失と、追加1 Mbit/s・3回無欠落は両方有効な観測であり、常に1%以下の損失となることまでは証明していない。

2 MHz上りは2.5 Mbit/sの60秒3回で15071/15085・15113/15115・15102/15110を受信した。合計24欠落を保持する。4 Mbit/s指定時のTX実績3.121515 Mbit/s、9755/9755、RX UDP/body 3.114509/3.072982 Mbit/sが同条件の最大観測値だった。無制限送信は9687/9695、UDP/body 3.092445/3.051213 Mbit/s。3 Mbit/s指定も送信追随92.38%で未達となり、ここでも送信pathの追随不足が合格候補を制限した。

2 MHz下りは2 Mbit/sの60秒3回で12138/12164・12187/12187・12193/12206を受信し、計39欠落。2.5 Mbit/s指定では送信追随93.59%で未達となった。無制限送信は8244/8244、UDP/body 2.631015/2.595935 Mbit/sで同条件の最大観測値だった。今回の4条件では、いずれも最高合格候補より上の探索点で送信追随不足を観測した。

## 実機で見つかった不具合

最初のsmokeでは、短いARM命令がUSBから分割して読み出された際に `fgets` の改行未到着をtoo_longと誤判定した。source `41929f5` の `smoke-1mhz-sta-to-ap` は測定開始前・stage0でFAILとなった。元ログを保存し、throughput値には含めない。

`b3964b5` は改行までbyteを蓄積し、一時的なEOFでも断片を保持する。真のbuffer上限を超えた場合だけ破棄するよう修正した。初回FAILでも両台のSTOP ACK・radio shutdown・DONEは記録されており、通信測定失敗と無線停止状態を分ける。

修正後smokeは128 kbit/s・5秒の5stageすべてで命令・計数・3秒drainが成立した。最初の4回は67/67、最後は61/67で6欠落し、短い確認試験の品質基準を満たさなかった。両端は5stageを完遂し無線停止・PASS・DONE。この有効な損失観測を保持し、条件を緩めず30秒探索／60秒確認へ進む。

最初の本測定 `full-1mhz-sta-to-ap` はstage10のRX summary待ちで9秒timeoutとなった。summaryは後続のcleanup出力が発生した時点で届き、7666/7666・drain3.029986秒を示していたが、3反復は完了していないため元のFAILを保持する。

このsummaryはCRLF込み512 Bで、USB packet64 Bの倍数だった。固定IDFのUSB Serial/JTAG VFSは、64 B終端の転送に0 B packetを追加しないとhost bufferに残ることを明記し、`fsync`の完了待ちで追加flushする実装を持つ。修正 `655c5bb` はsummary等の制御出力後にstdio flushとVFS fsyncを行う。

修正後1 MHz上りのstage10でも512 Bのsummaryとなり、7760/7760を記録。今回はDRAIN ACKからhost到着まで3.000秒、device drain3.029996秒で正常に取得できた。旧caseは同じ512 Bが約9.047秒後のcleanup出力まで滞留していた。host側もDRAIN ACKを確認し、同時capture済みのsummaryを捨てずに扱う回帰修正を追加した。

## 計数・性能の解釈

- TX成功だけを受信goodputにしない。送信失敗、受信損失、duplicate、tail、invalidを分離する。
- 負荷生成の独立skipped counterはなく、送信実績/目標rateで追随不足を判定する。送信pathの限界をRFだけの限界にしない。
- 低負荷で観測した損失の原因、および送信追随不足に対するESP32 CPU・lwIP・SPI・無線airtimeの寄与は未分離。今回の測定結果を残し、追加の性能最適化は行っていない。
- IDLE taskを動かすため、送受信loopは約50 msごとに1 tickのblocking yieldを行う。body検証やlwIP処理も含む今回の経路の実効値である。
- 有限時間・探索点の範囲に制約される実測で、PHY理論最大、TCP、音声遅延、長時間持続能力を証明するものではない。

8時間運転は未実施。実電源断50回は[Issue #27](https://github.com/rimtty/RoadWeave/issues/27)、必要な計測器購入・電源計測は[Issue #28](https://github.com/rimtty/RoadWeave/issues/28)の延期状態を維持し、P0-A全体を完了とはしない。

## 証拠と終了状態

[実測データ](logs/2026-09-19-throughput/README.md)に各stageの独立再解析、失敗capture、安全に抽出したeventsと原本hashを保存する。[firmware manifest](logs/2026-09-19-throughput/firmware-manifest.json)にはbinary SHA256・非秘密設定・個体別flash証拠を記録する。private原ログと初回実行tools snapshotも保持し、host checkoutを実機sourceと取り違えない。

[再実行手順](halow-throughput-runner.md)は幅・方向ごとに有限captureを作成し、STOPから正常停止まで確認する。追加探索は既存runnerのrate指定のみを変え、測定中にFW・判定実装は変更していない。

| 成果 | 実測・確認したsource | 保存 |
|---|---|---|
| 計画 | `9480d1c` | [PR #37](https://github.com/rimtty/RoadWeave/pull/37)、merge `0a618437` |
| stream firmware・USB入力/終端修正 | `655c5bb30ed44d33235f511105eac329ae50f058` | [PR #38](https://github.com/rimtty/RoadWeave/pull/38)、merge `6295102` |
| host探索・counter照合・summary受信修正 | `5a12d4dddf3cbe4675819fd9f96c584a967f123d` | [PR #39](https://github.com/rimtty/RoadWeave/pull/39)、merge `ed29310` |

最終sourceでAP/STA×1/2 MHzの4構成をbuildした。hostは正常・損失・不整合・期限・同時到着summary等の16テストを通過している。

終了時はCOM4/5を保存済みの通常 `aa58241` 1 MHz DHCP firmwareへ書き戻し、COM6は同sourceのSTA2を維持した。3台の160秒通信を再検査し、warmup後はSTA1 588/588、STA2 587/587、同時測定窓146.859秒でPASS。全3台のSTOP ACK・RUN_END・radio shutdown・PASS・DONEを確認した。[最終状態の証拠](logs/2026-09-19-throughput/final-state.json)に個体・binary SHA・復元flashの照合を残す。

現在は全3台無線停止。RF無効のfirmwareへ変更したわけではなく、次回reset／再給電すると通常の1 MHz DHCP試験が最大600秒の設定で始まる。最大速度測定用firmwareとは別のsourceであることを最終状態にも明記した。
