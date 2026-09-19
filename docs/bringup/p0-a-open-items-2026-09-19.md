# P0-A 到達点と残課題（2026-09-19）

判定: **DHCP・有限通信・復帰の実証を進めたが、P0-A全体は未完了。**
この文書は[ロードマップ](../poc-roadmap.md)、GitHub Issue #1/#2/#3、
[前段実測](halow-link-retest-2026-09-19.md)と[当日追加実証](p0-a-validation-report-2026-09-19.md)を照合した整理。
長時間試験は実施していない。cold bootと電源計測はユーザー指示で当日延期し、Issue #27/#28へ保存した。

## 確認できたこと

- 既知の3個体をUSB serial/MACで識別し、8 MiB Flash/PSRAMを認識。PSRAM起動検査PASS。
- 3組のMM6108 SPI起動、chip ID 100/100反復、bulk書込み・読出しがPASS。
- 3組でMM6108 FW起動とバージョン取得。COM4をAP、COM5/COM6を順番にSTAとしてWPA3-SAE接続。
- HaLow 903.5 MHz・1 MHz幅・1 dBm上限設定で、各組UDP 20/20往復。平均RTT 25.628 / 27.806 ms。
- チャンネル設定順序、BUSY未配線に対応するtransport sleep抑止、送信コールバックを修正。
- GPIO21のLED状態表示を3台へ書込み。実際の点滅の目視確認は未報告。
- 通信経路はESP32-S3 → SPI → MM6108 → HaLow。内蔵2.4 GHz Wi-Fiや一般的な2.4/5 GHz WLANは使わない。
- 当日追加: 有限継続モード、DHCP、peer別検証・統計、hostの厳格な判定とログ保存を実装。両STA順次および3台同時のDHCP通信を確認。
- AP software restartのwatchdog不具合を実機で発見し、起動IRQ無効化と停止処理を追加。controlled AP/STA再起動各3回、APサービス停止・再開3回の復帰を実証。元FAILも保存。
- 修正版で1/2 MHz各3回、追加1024 B/100 ms負荷、3台同時baselineと両STA交互再起動を実証。全3台を正常に無線停止した。

20回の短いUDP試験は、連続運転・音声遅延・通信距離・最大速度の証明ではない。
最大転送速度は別の[UDP stream実測](p0-a-throughput-report-2026-09-19.md)で検証し、60秒3反復の合格負荷と飽和時の最大観測goodput、損失変動を区別して記録する。
SPI/PSRAMの成功と、基板を外したXIAO単体smoke・外観/導通検査の証跡も区別する。

## 現フェーズで閉じる課題

| 順序 | 課題 | 現状と残作業 | 完了の確認方法 |
|---|---|---|---|
| 1 | 今回の成果を保存・再現可能にする | 前段成果PR #25、計画#26、機能別PRと当日結果へ保存。実機binary SHAとhost checkoutを分離して記録 | マージ実績・機能一覧は当日レポート。個体別設定とclean build、秘密はGit外 |
| 2 | 個体・受入記録を埋める | MACは確定。RW-N01〜03の物理ラベル、XIAO/HaLowの組合せ、外観・ヘッダー・U.FL、無通電導通検査、全台のXIAO単体smoke記録に欠落 | 写真・測定表・個体別ログを対応付けてIssue #1の残条件を埋める |
| 3 | 起動再現性 | 実電源断50回は当日延期。[Issue #27](https://github.com/rimtty/RoadWeave/issues/27)。電源制御設備なし、手動50回も実施不可 | 将来2組各50回、driver初期化成功100%。software resetを数えない |
| 4 | 継続接続・自動復帰・IP管理 | 有限継続とDHCPを実装し両STA／3台同時で成功。controlled AP/STA再起動各3回・APサービス再開3回の復帰PASS | 各faultのACK、fresh boot／lease、UDP復帰、正常終了を照合。サービス停止と電波伝搬によるlossを区別 |
| 5 | 8時間連続UDP | 当日の対象外。数分間の通信・heap記録は取得済みだが長期の検証は未実施 | 8時間の送受信数、欠落/重複、RTT分布、heap推移、再接続、reset reasonを記録 |
| 6 | ネットワーク基準測定 | 同一修正版・負荷で1/2 MHz各3回PASS。exact RTT・loss・goodput・RSSI・scan noise/SNR・raw RC・heapを取得 | 中心周波数と周囲雑音も異なる比較。scan SNRと継続SNRを区別。TX retry総数は公開SDK APIから未取得で、RC差分から捏造しない |
| 7 | 電源・電流 | 当日延期。[Issue #28](https://github.com/rimtty/RoadWeave/issues/28)。計測器未保有で購入が必要 | [機器と治具の必要仕様](p0-a-equipment-followups-2026-09-19.md)を参照。平均と時間分解能に応じたpeak、測定点、rail電圧を記録 |
| 8 | ボード固有対策・警告の扱い | BUSY未配線対策で通信成立。初期化時のtransport/未知TLV警告は残る | sleep抑止が必要な範囲・SDK版・電力への影響を記録し、長時間/再初期化でも検証。警告が回復可能な既知挙動か、追加対策が要るかを判定 |

関連: [Issue #1 受入](https://github.com/rimtty/RoadWeave/issues/1)、
[Issue #2 bring-up](https://github.com/rimtty/RoadWeave/issues/2)、
[Issue #3 UDP soak・復帰](https://github.com/rimtty/RoadWeave/issues/3)。

Issue #3の「AP/STAの接続、DHCP、UDP echo」は実証結果を個別に対応付け、8時間soakまで完了したとは扱わない。
当日bench用の受信率99%・復帰60秒以内などは[事前計画](p0-a-validation-plan-2026-09-19.md)に記載し、音声・製品基準とは分ける。
UDPのRTTを、そのまま音声のmouth-to-ear遅延として扱わない。

## 判定基準・文書の更新が必要な点

- ロードマップ/Issue #2の「Porting Assistant全項目PASS」は、標準WM6180のBUSY未配線と矛盾する。
  **基板適用項目のPASS＋BUSY/WAKE非適用の根拠＋sleep抑止状態での通信/復帰検証**へ見直す案を残す。
  今回のBUSY FAILを黙ってPASSへ置き換えたり、全項目合格と記載したりしない。
- 元のPorting Assistant全項目は実行していない。SPI診断・FW起動・無線通信の個別証跡で、どの項目を代替できるか明示する。
- README・scheduleの到着待ち/通信未成立の記述には古い日付のものがある。最新到達点へのリンクを設け、過去記録は履歴として残す。
- GitHub Issue #1/#2/#3は確認時点でOPEN。チェック欄とProjectの進捗・日付への反映が残る。
  この整理ではProjectボードの現在の列・期日を取得していないため、更新済みと扱わない。
- アンテナの型番、装着先、試験時の配置/距離・電源条件を再現用記録へ補う。
- GPIO21のLED表示は実装/書込み済み。接続待ち・接続中・通信・成功/失敗の目視確認を残す。

## 3台同時通信の位置付け

**推奨する追加試験だが、元のP0-A必須条件ではない。** 元計画は2台AP/STA＋交換診断用1台。
当日FWはAP最大2STA、固有ID、peer別echo統計、DHCPへ対応済み。
修正版150秒baselineでSTA1 `.2`／STA2 `.3` を取得。全期間576/576・570/571、共有warmup後136.844秒は548/548・547/547だった。
両STAを交互に再起動し、3.891/4.875秒で復帰。その間の相手の最大echo間隔は各0.547秒、全epoch合計870/870・866/866だった。
追加試験の基準を満たしたが、8時間安定性や音声遅延の証明とは分ける。

## 次フェーズに持ち越すもの

| 次工程 | 残課題 |
|---|---|
| P0-B 音声 | 現時点のマイク/アンプ/スピーカー/PTT/配線部材の到着状況を再確認。D11/D12の取り出し、I2S loopback、hard mute、既存RWP/音声処理のHaLow接続、音声p95 ≤150 msなど |
| 製品候補基板 | HC01 V2等での同じ試験、BCF/FW、電源、RF経路の検証。WM6180成功で代用しない |
| グループ・製品機能 | 6台規模、UI実機、GPS、録音、地図等。現在のHaLow基礎試験の完了条件に混ぜない |

音声部材は過去文書とユーザー申告の時点が異なり、9月19日の到着状況は確認していない。
[Issue #18](https://github.com/rimtty/RoadWeave/issues/18)の一覧を最新在庫と照合してからP0-Bへ進む。
過去の内蔵Wi-Fiによる音声ベンチは参考履歴に留め、今後の実機通信はSPI HaLowで行う。

## 残件の進め方

1. 当日結果のFAIL→修正→再試験と機能別PR、個体・binary対応を保存する。
2. 8時間soakは別途開始の判断を受けて実行する。今回は予約・開始しない。
3. Issue #27の電源断治具とIssue #28の測定器を準備し、実電源断50回と電流・電圧を実測する。
4. 現物受入・アンテナ配置・LED目視の不足を補い、部材在庫を確認してP0-Bへ進む。

本整理の更新は長時間試験の開始指示やスケジュール登録ではない。当日の機器操作と最終状態は実証レポートに記録する。
