# HaLow アンテナ装着後の通信試験（2026-09-08）

**2台ともMM6108のFW起動に成功したが、STAの接続はタイムアウトし、UDP通信は未成立。**
ユーザーの終了指示で実機試験を停止した。2回の試行とも両台のログで
`RW_LINK_RADIO_SHUTDOWN`と`RW_LINK_DONE`を確認している。

## 試験条件

ユーザーからRFアンテナ装着済み、および「米国など、使用条件を確認済みの場所」と申告を受けた。
2台ともWM6180側のアンテナ端子に接続しているかは、確認質問への回答待ち。

| 項目 | AP | STA |
|---|---|---|
| USB | COM4 | COM5 |
| ESP32 MAC | 44:b1:76:b0:57:20 | 44:b1:76:b0:57:1c |
| HaLowデバイスMAC | 3c:22:7f:71:df:07 | a8:dd:9f:4d:c1:ea |
| APインターフェースMAC | 3e:22:7f:71:df:07 | — |
| IPv4 | 192.168.50.1/24 | 192.168.50.2/24 |
| 動作 | UDP 3333、120秒間echo待機 | 接続とIPv4を30秒待機、成立後20回echo測定 |

- 国設定US、S1G channel 3 / operating class 1（SDK regdbでは903.5 MHz、1 MHz幅）。
- 最大送信出力overrideは1 dBm。これはソフトウェア設定であり、実測値ではない。
- SSID `RoadWeaveBench`、WPA3-SAE。共通のランダムPSKはローカルの生成sdkconfigのみで保持。
- ESP-IDF v5.4.4、`morsemicro/halow` 2.11.2-esp32-2、ESP32内蔵2.4 GHz Wi-Fiは未使用。
- GPIO: RESET_N=1、CS=4、IRQ=3、SCK=7、MOSI=9、MISO=8。BUSY/WAKE未配線のため省電力無効。

## 実測結果

両台ともchip ID `0x0306`、Morse FW `1.17.8`、Morselib `2.11.2`を取得。
BCF API `8.0.0`、board description `mf16858`を表示した。

| 試行 | AP | STA | 結果 |
|---|---|---|---|
| 1 | AP_READY、echo数0、送信時VIF選択エラー | 接続30秒タイムアウト、bits=0 | 両台FAIL、正常に試験終了 |
| 2 | AP netifのMACをAP VIFのMACに修正、echo数0、VIF選択エラーは再発 | スキャン開始・完了を繰り返すが対象AP検出ログなし、接続タイムアウト | 両台FAIL、正常に試験終了 |

STAはUDP送信処理に到達していないため、パケット損失率・RTT・通信距離は測定できていない。
AP起動やSPI検査の成功を無線接続成功と扱わない。

## 保存した変更と検証範囲

[実験用ファームウェア](../../firmware/experiments/halow_link/README.md)は通常の製品FWと別プロジェクト。
RFは既定で無効、PSK・channel・operating classが未設定なら通信試験を拒否する。

最後の実機試行後、以下を実装してAP/STA両方のRF有効構成をビルド・リンクした。
**以下の変更は未書込みで、実機検証は翌回に持ち越し。**

- 送信metadataにAP/STAのVIFを明示し、`Unable to infer VIF ID`への対策を追加。
  SDKのドライバーハンドルとRXバッファ解放方式を保持して送信コールバックを差し替える。
- 両台のチャンネルリストを指定チャンネル1つに絞り、周波数・帯域・設定結果を記録する。
- APの端末認証状態、STAの探索・接続イベントを記録する。INFOログを有効にし、バイナリダンプの内容は省略する。
- 本体ユーザーLED（GPIO21、active low）による状態表示を追加。

| LED | 意味 |
|---|---|
| ゆっくり点滅 | 起動中・相手との接続待ち |
| 点灯 | STA接続成立／AP側でSTAのデータ通信認証成立 |
| 短く消灯 | 正しいUDP echoの受信／返信 |
| 2回点滅を繰り返す | 試験成功、無線停止後 |
| 3回点滅を繰り返す | 試験失敗、詳細はシリアルログ |

LEDは[Seeed公式のGPIO21・active low仕様](https://wiki.seeedstudio.com/xiao-esp32s3-freertos/)に基づく。
光り方の目視確認は未実施。RF無効構成ではLEDの最終結果もpreflightの結果を表す。

最終ビルドのアプリサイズはAP `0x170320`、STA `0x170a70`。ともに3 MiBアプリ領域内に収まる。
その前のpreflight実機検査は[別記録](halow-link-preflight-2026-09-08.md)を参照。

## 次回の再開点

1. USBポートとMACを再確認し、2台のWM6180側アンテナ接続を確認する。
2. 最終ソースをビルドし、AP/STAを再書込みする。AP_READYを待ってからSTAを起動する。
3. 単一チャンネルの探索結果・ドライバーログから未接続原因を調べ、UDPの20往復を確認する。
4. LED表示を確認する。長時間連続運転・再接続・スループット測定は、その後の別試験。

終了時点で実機には試行2のRF有効FWが残る。現在の試験は終了済みだが、
リセットやUSB再接続をすると自動で次の試験が始まる。最終ソースのLED表示はまだ実機に入っていない。

## 証跡

- [試行1 AP](logs/2026-09-08-halow-link-ap-rf-attempt1.log) / [試行1 STA](logs/2026-09-08-halow-link-sta-rf-attempt1.log)
- [試行2 AP](logs/2026-09-08-halow-link-ap-rf-attempt2.log) / [試行2 STA](logs/2026-09-08-halow-link-sta-rf-attempt2.log)

保存ログはシリアル出力の改行・ANSI制御・行末空白を正規化した。生ログ、書込み・ビルドログ、
再開メモはローカルの`.private/halow-antenna-test-20260908/`に保存。
生成sdkconfig・PSK・バイナリ・全FlashバックアップはGitに含めない。
