# HaLow 2台通信テストプログラムの事前検査（2026-09-08）

この文書はアンテナ到着前の検査記録。後続の実機状態と通信試験結果は
[アンテナ装着後の試験記録](halow-link-antenna-test-2026-09-08.md)を参照。

`firmware/experiments/halow_link`にAP/STA・UDP echoテストを用意し、接続中の2台へ**送信無効のpreflight構成**を書き込んで実行した。
両台でSPIと組込みFW/BCFファイルの検査はPASS。**HaLowの相互接続・UDP往復は未実施**。
アンテナ・終端・減衰器などのRF試験部材は未到着のため、無線FW起動、スキャン、AP beacon、STA接続は実行していない。

## 実機結果

| 項目 | AP役 | STA役 |
|---|---|---|
| USB | COM4 | COM5 |
| ESP32 MAC | 44:b1:76:b0:57:20 | 44:b1:76:b0:57:1c |
| PSRAM | 8 MiB、memory test OK | 8 MiB、memory test OK |
| MM6108 chip ID | 0x0306、100/100一致 | 0x0306、100/100一致 |
| FWコンテナ | PASS、EOF前399,792 bytes、22 TLV | 同左 |
| BCFコンテナ | PASS、EOF前536 bytes、12 TLV | 同左 |
| `RW_LINK_PREFLIGHT` | PASS | PASS |
| `RW_LINK_RADIO_RESULT` | NOT_RUN | NOT_RUN |
| 終了状態 | RESET_N LOW、診断FWを保持 | 同左 |

FW/BCFのPASSは組込みファイルのmagic・TLV長・読出し・EOFまでの構造検査であり、無線チップへのロード、起動、較正内容の正しさを証明するものではない。
BUSY/WAKE未配線の標準基板向けに省電力を無効化。以前の全Flashバックアップはそのままローカルに保持し、新しいパーティション表でもNVSのオフセットとサイズを保持した。

## テストプログラム

- ESP-IDF v5.4.4、Morse component `2.11.2-esp32-2`を固定。
- 正式なソースとビルド手順: [halow_link README](../../firmware/experiments/halow_link/README.md)。
- 通信モードはAP `192.168.50.1`、STA `192.168.50.2`の固定IPv4。今回はDHCPを含めない。
- APが120秒間UDP echoを提供し、STAが接続とIPv4準備を待って20回のUDP往復を測定する。
- STAは送信ごとのnonce/sequenceが完全一致する応答だけを成功とし、受信数・平均/最大RTT・タイムアウトを報告する。
- 通信モードは明示的なRF有効化とSSID/PSK/channel/op-class設定が必要。実機に書いた構成はRF無効、PSK空、channel/op-class未設定。
- RF試験条件が整うまではpreflightとして使用する。ビルド成功やpreflight PASSを通信成功として扱わない。

## 実機に書き込んだアプリ

| 役割 | サイズ | SHA256 |
|---|---:|---|
| AP preflight | 636,720 bytes（0x9b730） | `3ce0d4288d87fa82d280c90e9a718d95e28537fca4665044bd66a69eee172c5c` |
| STA preflight | 636,720 bytes（0x9b730） | `b1d83bf2a03442eac296a4172a8dfa4280c65b41165e12e5d1671c5b91893af8` |

## ビルド検証

AP/STAそれぞれでpreflightとRF有効の両構成をビルド・リンクできた。
RF有効の検証用バイナリはAP 1,447,216 bytes、STA 1,449,584 bytesで、実機には書き込んでいない。
検証用の仮設定を用いて通信コードがリンクされることを確認したものであり、channel/op-classやRF条件の妥当性を実証した試験ではない。
検証後は生成sdkconfigを送信無効の設定へ戻し、AP/STAともpreflightを再ビルド済み。

実機用ログのPASS/NOT_RUN表示、ビルド設定のRF無効・省電力無効、記録内の相対リンクを確認した。

## ログ

- [AP preflight](logs/2026-09-08-halow-link-ap-preflight.log)
- [STA preflight](logs/2026-09-08-halow-link-sta-preflight.log)

生ログ・書込みログ・実機用アプリの複製・ビルド検証の設定とログは`.private/halow-link-test-20260908/`に保存。
