# XIAO ESP32-S3 + SPI + Wio-WM6180 最大性能の調査・改善計画

作成日: 2026-09-19。状態: **段階的な検証計画。新しい実機測定結果ではない。**
対象はXIAO ESP32-S3からSPI接続したSeeed Wio-WM6180 / MM6108までの経路。
内蔵2.4 GHz Wi-Fiを代替経路として使わない。今回の計画・履歴整理・offline toolkit整備と、将来の書込み・RF試験を別の完了条件で管理する。

## 既知の値と、まだ分からない上限

[2026-09-19実測報告](p0-a-throughput-report-2026-09-19.md)の最大観測は
**2 MHz・STA→APのUDP payload 3.115 Mbit/s、検証body 3.073 Mbit/s**。
これは1/2 MHzの限られた設定と負荷生成器で測った値であり、このハードウェアの最高性能ではない。
最高合格目標は同条件の2.5 Mbit/s、60秒×3の受信UDP goodputは2.411–2.417 Mbit/s。
目標rate、実送信rate、受信goodput、PHY rateを同じ列に混ぜない。

| 根拠 | 確認した内容 | 未確認・比較の制約 |
|---|---|---|
| 過去の1/2 MHz×両方向 | 全4条件で高負荷側の送信追随不足を観測 | CPU、lwIP、SPI待ち、RF backpressureの寄与は未分離 |
| 過去のSPI診断 | raw転送7.522 Mbit/s、CRC/bulk検査PASS | 診断の転送長・経路に依存。無線goodputでもSPI物理上限でもない |
| 過去のthroughput設定 | SPI設定40 MHz、1200 B UDP、1184 B body、SDKログWARN | 実SCKの測定と実通信中の有効SPI転送量は別途必要 |
| MM6108公式datasheet | 1/2/4/8 MHz、MCS 0–7および10、最大32.5 Mbpsは8 MHz・MCS7・短GIのPHY値 | WM6180のRF部品・BCF・地域設定・ESP32 SDK/FWで全条件が利用可能とは限らない |

チップ仕様は[Morse Micro MM6108 Data Sheet v104, §1.2](https://www.morsemicro.com/resources/datasheets/chips/MM6108_Data_Sheet.pdf)を確認した（2026-09-19参照）。
別世代MM8108の256-QAM/MCS8・9や43.3 MbpsをMM6108に転記しない。

比較資料として、メーカーcommunityの実験者tharveyはi.MX8MP + SDIO / driver 1.12.4で、
Morse rate controlを有効にした後、**2 MHzでiperf3 UDP約6.8 Mbps、TCP約6 Mbps**と報告している。
これは外部ホストでの本人の測定報告で、製品保証値ではない。記事後半にi.MX8MMという表記もあるため、厳密なホスト型番は記事内で不一致として保持する。
SPI版XIAOへの転用、同じloss基準・測定窓の仮定、達成必須の合格値への採用はしない。
ただし「2 MHzだから3.115 Mbpsが限界」という推論を退け、rate control確認を優先する根拠になる。
出典: [What is maximum speed of MM6108, 投稿3/5（2025-03-04/06）](https://community.morsemicro.com/t/what-is-maximum-speed-of-mm6108/241)。

## 段階と終了成果

| 段階 | 作業と成果 | 次へ進む条件 |
|---|---|---|
| A: offline整備 | 過去結果を再解析、証拠schema、比較・候補計画・不足項目一覧を作る | 過去の最大/反復合格/失敗を同じ意味で再現。実機を触らず完了可能 |
| B: 送信path診断 | 既知の1/2 MHzでCPU、生成器、SPI、queue、rate controlを計測 | 有効な両端計数と、少なくとも律速の候補・根拠・未観測項目を記録 |
| C: 対照baseline | SDK iperf例と既存runnerを同じRF条件で比較。UDP、可能ならTCP | protocol互換性と測定単位を確認。比較不能なら理由を明記 |
| D: 4/8 MHz対応 | チップ以外の対応条件を調査し、4 MHz、次に8 MHzを個別に試す | 下記の対応・地域・実動作確認gateをすべて満たす |
| E: 改善と確認 | 一要因ずつ変更しbaselineと交互比較、採用候補を反復 | 有効性・損失・再現性・停止を維持した改善、または根拠付きの打切り |

4/8 MHzへ広げる前にBを行う。送信側が負荷を生成できないまま帯域だけを増やすと、ホスト制約をRF上限と取り違える。
今回Aを終えてもB–Eの実機結果や最大性能確定を意味しない。

## B: 送信pathとrate controlを先に分離する

既存の検証FW、AP/STA、DHCP、board compatibility対策をbaselineとして保存する。
両方向を別sessionで測り、配置・電源・アンテナ・ログ量・CPU設定を固定する。
最初は1/2 MHzの既存合格rateと直上の不合格rateで比較し、全面sweepを繰り返さない。

| 層 | 記録する観測 | 一要因の比較例 |
|---|---|---|
| 負荷生成 | pacing予定数、実attempt/s、成功数、独立skipped数、wake遅延、sendto所要時間の分布 | body生成の事前計算。検証なし版は診断専用として通常goodputと区別 |
| CPU / RTOS | 各core/主要taskの実行時間、idle、task priority/affinity、周波数、stack/heap最小値 | 計測有無の対照、適正範囲内でtask配置やbuffer位置を1つずつ変更 |
| lwIP / memory | socket待ち、送信error、pbuf不足、queue depth/high-water、copy量、内部RAM/PSRAM配置 | socket/pbuf設定の単独変更。メモリ枯渇とdropを隠さない |
| SPI / transport | 実SCK、TX/RX bytes・時間、有効payload量、transaction長・間隔、DMA、IRQ→処理遅延、queue待ち | RFを使わないbounded bus試験と実通信中の計測。CRC/timeout数も記録 |
| MAC / RF | 実幅、MCS分布、GI、rate control状態、aggregation、再送、RSSI/noise、channel busy | 取得可能な公式APIで確認。取得不能はunknown。対応範囲内の固定MCSは診断用 |

40 MHzという設定値だけから40 Mbit/sのアプリ転送を期待しない。実SCKはdriver取得値と計測器実測を区別する。
SPIクロック変更はESP32 peripheral、MM6108、module/配線の仕様と安定性範囲内に限り、現設定での空き時間・DMA・転送粒度を先に調べる。
RF受信待ちがsendtoへ伝わる場合もあるため、sendto停滞だけでCPU律速とは判定しない。

約50 msごとのblocking yield、検証body、bitmap、USB集計出力についても負荷を測る。
watchdog/idleを無効にして得た数字は採用しない。profilingの有無で性能が変わる場合はその差を併記する。
BUSY/WAKE未配線を前提とした既存shimは維持し、省電力変更で受信IRQを止める経路へ戻さない。

固定component `morsemicro/halow 2.11.2-esp32-2`、Morse FW 1.17.8、ESP-IDF 5.4.4を出発点とし、
rate controlの実装・有効設定・実MCSをsource/APIで照合する。Linuxのdebugfsやmodule parameterがESP32で使えるとは仮定しない。
FW/SDK更新を試す場合は独立条件にし、同時にbandwidthやCPU設定を変更しない。

## C: iperf対照とRF条件

固定SDKの `morsemicro__halow/examples/iperf` と `espressif/iperf-cmd` の実version・protocolを確認する。
同例のREADMEはSeeed XIAO ESP32-S3 + XIAO MM6108 profileと、STA起動後のserial REPLによるUDP/TCP RX/TXを案内している。
iperf2系とiperf3は互換とは仮定せず、両端の実装・version・command・結果formatを記録する。
既存AP/DHCP/WM6180対策を保持して通信可能にする工程を先に行い、単独STA例のbuild成功だけで対照測定を完了しない。
現行RoadWeave APはiperf serverではないため、対応する相手側iperf endpointを準備し、同じHaLow経路・RF条件にそろえる必要がある。

- 同じ1/2 MHz・方向・1200 B UDP（対応する場合）で、warmup5秒、測定60秒×3を実施する。送信値ではなく受信値を使う。
- iperfにbody検証・active/tail分離がない場合は独自runnerの合格基準を満たしたと扱わず、対照観測として並べる。
- TCPは可能な実装で単一stream・両方向を別々に測る。受信application bytes/実時間、window/buffer、再送とCPUを記録する。未対応なら未実施理由と追加実装を残す。
- 適切なアンテナ配置・距離・姿勢を固定し、可能なら適合するRF fixtureと減衰器で再現性を上げる。過大入力を避け、減衰量は両端の仕様に基づき決める。
- 各stageのRSSI/noiseとMCSを記録し、受信状態が異なる対照を同一条件と扱わない。周波数が変わる1/2/4/8 MHz比較は幅だけの因果効果としない。

## D: 4/8 MHzの対応・運用gate

チップの公式対応は必要条件の一つにすぎない。次をAP/STA両端について表にし、未確認項目は `unknown` とする。
現在のRoadWeave throughput runnerの幅指定は1/2 MHzに限定される。MM6108の4/8 MHz対応と、runner/board/FWが4/8 MHz試験を実行できることは別である。

1. Wio-WM6180の実module/SKU/基板revision、RF front end、アンテナ、BCF名・hash、FW/SDKが候補幅・周波数を扱えること。
2. 実際の試験場所・moduleの条件に合うcountry、channel、operating class、primary/operating bandwidth、中心周波数、出力、アンテナ条件。
   過去のUS 1/2 MHz設定・1 dBm overrideは任意の4/8 MHz設定を許可する証拠にならない。channel番号を幅から推測して作らず、固定SDKのchannel tableとmodule資料で照合する。
3. RF無効のpreflightで候補を検査し、build・設定受理を確認する。これらはRF動作確認とは区別する。
4. 実機段階で選択channelのログ、AP advertisement、STAが接続した実幅を確認する。4 MHz指定で2 MHz接続なら4 MHz測定成功にしない。
5. 4 MHzの低負荷smoke・両方向有限測定・正常停止を完了してから8 MHzへ進む。未対応ならその条件だけ `unsupported` / `blocked` と理由・根拠を残し、既知条件の診断を継続する。

法規や認証の詳細はこの計画で新たに断定しない。試験前に該当SKUの最新公式資料と実施条件を確認し、根拠の参照先を記録する。
日本向け適合性と北米referenceの性能結果は[ADR-0002](../decisions/0002-wm6180-reference-platform.md)どおり分離する。

## E: 探索、採用、停止のルール

主指標は既存と同じ「指定負荷に追随した反復合格」と「最大観測receiver goodput」の二本立て。
UDPはactive/final受信率99%以上、実TX/目標95%以上、DATA窓が要求の99%以上、60秒×3すべて合格を維持する。
body不正・stage/peer混入・counter矛盾・overflow・予期しないreset/切断・必須marker欠落は `invalid` であり、低性能の有効測定とは分ける。
tail、duplicate、out-of-order、send error、skippedを別記し、合格へ都合よく足し戻さない。

1 MHz下りでは低負荷2% lossと高負荷無欠落が両立した。
将来の探索は最初の品質不合格だけを絶対上限にせず、事前に固定した小さな粗いgridと各点の反復から非単調性を確認する。
不合格点も保持し、追加点・追加回数・時間上限を実行前に固定する。結果を見て際限なく再試験しない。
既存runnerの初回不合格で止まる探索を使う場合、その結果はその探索policy内の最高合格点と明記する。

改善の採用はbaselineと候補を交互に3組以上比較し、同じ判定で合格し、代表goodputが5%以上向上し、差が反復のばらつきで説明されないことを暫定基準とする。
差が小さければ「同等/未確定」。不正packet・reset・メモリ不足・停止失敗が増える案は採用しない。
最良候補の長めの確認を行う場合も、事前に時間とpacket上限を固定した別sessionとし、8時間耐久やP0-A完了へ読み替えない。

探索点上限まで合格なら `search_ceiling`、生成器が追随不能なら `sender_limited`、帯域未対応なら `unsupported` とする。
CPU使用率・SPI占有率・MCSのどれか1値だけで律速を断定せず、対照変更により律速候補が動く証拠と他層の余力を合わせる。
証拠不足なら `unattributed` を正しい結論とする。分かったのは特定構成の運用範囲であり、全hardwareの絶対上限ではない。

各sessionはhost 1500秒/firmware 1800秒以内など検証済みの有限上限を維持する。
高rate・長時間では262144 sequence上限を送信前に検査し、分割または別途検証した容量変更を必要とする。
STOP、両端ACK、session-end、radio shutdown、DONEを確認できなければ停止状態を `unknown` とし、合格終了にしない。
熱・電源異常、CRC/transport異常、連続reset、制御喪失では負荷増加を停止し、既知構成へ戻す。

## Toolkitと証拠の契約

最初のtoolkitはoffline解析・比較・実験候補manifest生成まででよい。候補生成は対応確認・書込み・RF実行を意味しない。
既存 `tools/halow_throughput.py` の測定定義と、公開された
[metrics.json](logs/2026-09-19-throughput/metrics.json)、[manifest](logs/2026-09-19-throughput/manifest.json)を入力として扱う。
以下は将来拡張の論理schemaであり、この計画だけで全項目が実装済みとはしない。

| 証拠群 | 保存する項目 |
|---|---|
| 由来 | schema_version、run/case/stage ID、日時、host/source/analysis commit、dirty状態、tool hash、AP/STA binary SHA256、flash照合状態、raw/event hash |
| 条件 | 個体/role、board/module、SDK/FW/BCF version/hash、RF条件、幅の要求/実際、SPI設定/実測、CPU、ログ設定、protocol/version、payload/body、変更した1要因 |
| 計測 | phase、目標/実送信rate、TX/RX counters、requested/actual duration、active span、UDP/body goodput、active/final loss、tail、3反復の個別値 |
| 診断 | MCS/GI/rate control、CPU/task、SPI/IRQ/queue、メモリ、取得方法と時間窓、計測負荷、unknown理由 |
| 判定 | 構造validity、品質判定、反復合格目標、観測最大、search ceiling、律速候補・証拠、未実施/未対応理由、終了と再起動時挙動 |

既存schemaにない項目はnullと理由を付け、設定値を実測値へ昇格させない。
future schemaの値を旧ログから推測して埋めず、正規化adapterのversionを記録する。
比較表は1/2 MHz×両方向ごとに過去/候補の値、測定定義、変更条件、比較可否を示す。
iperf・TCP・検証省略版の値は別系列。外部6.8 Mbpsはreference系列とし、内部PASSランキングへ混ぜない。
warmup、失敗capture、途中終了、旧FWの値を最大値選択へ混入させない。
host現在HEADは実機FW sourceの代用にならず、binを指定しただけでもflash照合済みとはしない。

公開出力はallowlistで構成し、PSKや秘密設定を含めない。raw logはprivate保存、公開側にはhashと安全な抽出証拠を置く。
通信に無関係なprivateファイルを収集しない。停止結果と品質結果を別々に保存する。

offline整備の受入れは、過去の3.114509 Mbit/sを丸めて3.115と再現し、2.5 Mbit/s反復合格目標と区別できること。
既存の失敗・損失・非単調例を保持し、欠落証拠・schema不一致・壊れたcounterを成功にしないことを確認する。
新しい実機結果がなければ「計画/解析ツール完成、性能改善は未測定」と報告する。

## Branchと独立レビュー

計画、既存知見の整理、toolkitを機能別feature branchで作り、各成果をレビューしてmainへ統合する。
本計画のbranchは `codex/halow-max-performance-plan`、起点はmain `fb5c6ac`。
実機FW変更とその測定結果は将来の独立branchで対応source/binaryを記録する。

既存report/runnerの独立レビューでは、unique body検証・active/tail分離・停止確認・source/hash保持は継承する。
追加必須事項は、初回lossで決めた境界の解釈、生成器のskippedと所要時間、実SPI/CPU/MCS情報、iperf互換性、4/8 MHzの実動作確認。
これらが未取得である現状からCPU/SPI/RFのいずれかを原因として確定しない。
計画と実装の差分（例: 診断項目が未実装）を成果報告で明示し、未取得項目を無視して完了扱いにしない。
