# HaLow最高性能調査: 現時点の記録（2026-09-19）

**状態: 調査計画と計測Toolkitを整備。新しいRF実測は未実施。**
対象はXIAO ESP32-S3とSPI接続したSeeed Wio-WM6180（MM6108）の経路であり、ESP32内蔵Wi-Fiは使わない。
[段階的な実証計画](halow-max-performance-plan-2026-09-19.md)に、今後の判定、証拠、停止条件を固定した。

## 既存結果から確定していること

[前回の実測](p0-a-throughput-report-2026-09-19.md)では、1/2 MHz・上下4条件を測った。
最大観測は2 MHz STA→APのUDP payload **3.114509 Mbit/s**（body **3.072982 Mbit/s**）。
2.5 Mbit/s指定で60秒×3回の品質条件を満たし、実際の受信UDP速度は2.411–2.417 Mbit/sだった。
4条件とも高負荷側では目標への送信追随が先に不足し、CPU・lwIP・SPI・RFの寄与は未分離である。
1 MHz下りでは128 kbit/s時に2%損失し、別の1 Mbit/s反復では無欠落だった。最初の不合格点を上限と決める探索は適さない。

[MM6108公式データシート](https://www.morsemicro.com/resources/datasheets/chips/MM6108_Data_Sheet.pdf)は1/2/4/8 MHz対応と最大32.5 Mbit/sの**PHY値**を記す。
[Morse Micro communityでの別機材の実測](https://community.morsemicro.com/t/what-is-maximum-speed-of-mm6108/241)には2 MHzのUDP 6.8 Mbit/sがある。
これはi.MX8系ホストのSDIO測定であり、本構成の達成値・保証値ではない。
現在の3.114509 Mbit/sをWM6180やESP32-S3の絶対上限とは扱わない。

## 今回統合したToolkit

| 成果 | 内容 | 統合 |
|---|---|---|
| [調査計画](halow-max-performance-plan-2026-09-19.md) | 既知とunknownを分け、送信path診断、SDK iperf対照、4/8 MHz、改善比較の順と受入れ条件を定義 | [PR #41](https://github.com/rimtty/RoadWeave/pull/41) |
| [offline解析](halow-throughput-analysis.md) | 保存済みstageの目標/実送信/受信率/観測最大を分離し、公開eventsから全counterとpeer証拠を再照合 | [PR #42](https://github.com/rimtty/RoadWeave/pull/42) |
| [firmware診断](../../firmware/experiments/halow_link/README.md) | 送信生成、`send()`、pacing、受信待ち、payload検査の段階別所要時間と接続後チャネルを記録 | [PR #43](https://github.com/rimtty/RoadWeave/pull/43) |
| [有限runner](halow-throughput-runner.md) | 4/8 MHzで両端の実動作チャネル証拠を照合し、幅別探索grid・sequence容量・観測時間を制限 | [PR #44](https://github.com/rimtty/RoadWeave/pull/44) |

公開された8 caseを新しいoffline解析器で再照合し、保存済みstageとeventsの差は**0件**だった。
2 MHz上りの3.114509 Mbit/sを元eventsから再計算した。改変した送信counterを検出するテストも通した。
AP/STA両方のthroughput profileをESP-IDF 5.4.4でビルドし、統合後のthroughput/validation関連**57件のPythonテスト**が通過した。
コード変更PR #42–44のhost CIとcompile-only CIも成功した。新しい診断値は実機でまだ採取していないため、計測による負荷と実環境での接続後チャネル取得は次の有限試験で確認する。

過去記録の再解析は、repository rootから次のように実行する。

```powershell
python -B tools/halow_throughput_analyze.py docs/bringup/logs/2026-09-19-throughput/metrics.json --events-dir docs/bringup/logs/2026-09-19-throughput
```

生ログ・認証情報はprivate領域に残し、公開側にはallowlist済みevents、hash、非秘密の集計だけを保存する。
今回、3台へのflash、COMアクセス、無線送信は行っていない。端末の最終**実測済み**状態は[前回の終了記録](p0-a-throughput-report-2026-09-19.md#証拠と終了状態)を参照する。

## 次回の実機調査で判定すること

1. 既知の1/2 MHz条件で新しい診断付きFWを少数の有限stageで検証し、旧値との差と計測負荷を記録する。`send()`停滞、pacing遅延、受信検査時間を分けても、単独の時間値からCPU/SPI/RF律速を断定しない。
2. SDK標準iperfとの対照を同じ相手端末・周波数・幅・距離で準備する。現行APはiperf serverではないため、endpointとprotocol互換性を先に確認する。可能ならUDPに加えTCPも記録する。
3. module/BCF/FWと試験場所で4 MHz、次に8 MHzが使える条件を確認し、両端の`RW_LINK_OPERATING_CHANNEL`が要求幅・チャネル・classと一致した場合にのみ低負荷smokeから有限測定へ進む。
4. 一要因ずつ調整し、baselineと候補を交互に比較する。速度、受信率、送信追随、停止結果を併記し、探索上限と機器上限を混同しない。

これらB–E段階は未実施。CPU/task計測、実SPI占有・SCK、MCS/GI分布、rate controlの実状態も未取得である。
8時間運転は別課題。実電源断50回に必要な機材は[Issue #27](https://github.com/rimtty/RoadWeave/issues/27)、電力測定器は[Issue #28](https://github.com/rimtty/RoadWeave/issues/28)に保持する。
