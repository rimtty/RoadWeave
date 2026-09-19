# 2026-09-19 P0-A 実証データ

[結果本文](../../p0-a-validation-report-2026-09-19.md)と併読する。18件の有限caseがPASS、修正前の実機FAIL1件を保持する。別記のlegacy回帰2件もPASS。

- `metrics.json`: 成功／失敗case、実機source・binary、最終epochと全epoch、exact RTT、DHCP、scan SNR、警告、復帰証拠。
- `manifest.json`: 公開eventsとprivate原eventsのSHA256、公開データからの再解析判定。
- `firmware-manifest.json`: 保存binaryの独立SHA256照合結果と明示allowlistによる非秘密設定。
- `final-devices.json`: 最終書込みの個体・binary・書込み検証と、全3台のSTOP・無線停止・DONE。
- `legacy-regression.json`: 両STAの20往復回帰。既知markerと数値のAP_ECHOEDだけを抽出し、sequence0〜19・両端終了を照合。host時刻は未記録。
- `*.events.jsonl`: `halow_validation.py`の既知marker・fieldと、生成された操作記録だけを抽出したログ。自由文errorは固定文へ置換。

SSID/PSK、private sdkconfig、SDKの生ログ、完全なローカルパスは含めない。生ログはprivateに保存しhashで対応付ける。
`dhcp-ap-reset3`は中断によりeventsがない実FAILで、metricsと本文のraw由来summaryを根拠とする。欠落時刻を合成しない。
`static-baseline`は旧harnessの準備boot混入FAILを保持し、各端末の最初の明示resetを境界に再解析した結果を掲載する。

RTTの公開exact値は個別echoのnearest-rank。FW summaryのpercentileは1 msのfloor histogramで、別指標。
warmup境界は最初の10秒以上SAMPLE。直後のechoが同じhost時刻を持つ場合もあるため、RTT抽出はイベントの記録順序で区切る。
`reconnects`はアプリ応答回復も含み、L2再接続回数とは限らない。
`last_epoch_summary`は最後のboot/runだけ、`summed_epoch_counts`は記録された全summaryの合計。
RC sent−successをTX retry数にしない。scan SNRはprobe受信時の推定で、通信全期間のSNRではない。

公開eventsは元のparserで再解析し、元caseと同じ判定を確認する。製品基準や8時間安定性を保証するデータではない。
公開eventsはLF改行に固定し、manifestのSHA256がGit保存内容とWindows checkoutでも一致するようにする。
