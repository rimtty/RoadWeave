# HaLow SPI・無線通信再試験（2026-09-19）

**3台ともSPI診断PASS。COM4をAPにして、COM5・COM6を順番にSTAとして接続し、
両組ともUDP 20/20往復に成功した。** 3台同時通信の試験ではない。
通信にはSPI接続のMM6108だけを使用し、ESP32内蔵2.4 GHz Wi-Fiを初期化していない。
一般的な2.4/5 GHz WLANは使わない方針をユーザーと再確認した。

## 機器・条件

| ポート | ESP32 MAC | 今回の役割 |
|---|---|---|
| COM4 | 44:b1:76:b0:57:20 | AP、192.168.50.1/24 |
| COM5 | 44:b1:76:b0:57:1c | 1組目のSTA、192.168.50.2/24 |
| COM6 | 44:b1:76:ae:c4:90 | 2組目のSTA、192.168.50.2/24 |

アンテナ入手・接続はユーザー申告。以前に申告された使用条件確認済み環境での試験として継続した。
US channel 3 / S1G operating class 1、903.5 MHz、帯域1 MHz、WPA3-SAE、送信出力上限override 1 dBm。
出力とアンテナ利得は実測していない。共通PSKはローカル生成設定のみで保持。
ESP-IDF v5.4.4、`morsemicro/halow` 2.11.2-esp32-2、Morse FW 1.17.8。

COM6のSPI結果は、この通信試験の直前に同日実施した状態確認の結果を使用した。
COM4・COM5は現在のアプリ領域3 MiBを読出し、実機とのdigest一致を確認してから診断FWを書き込んだ。
COM6のSPI診断FWと元の全Flashバックアップも従来のローカル保存物で復元可能。

## SPI結果

| 検査 | COM4 | COM5 | COM6 |
|---|---|---|---|
| MM6108 ID | 0x0306 | 0x0306 | 0x0306 |
| CRC付きID反復読出し | 100/100 | 100/100 | 100/100 |
| bulk書込み・読出し一致 | PASS | PASS | PASS |
| raw SPI転送速度 | 7,522 kbit/s | 7,522 kbit/s | 7,522 kbit/s |
| PSRAM 8 MiB検査 | PASS | PASS | PASS |
| BUSY端子検査 | FAIL | FAIL | FAIL |

BUSYのFAILは[既知のBUSY/WAKE未配線](wm6180-spi-2026-09-08.md)と整合する。
物理GPIOのBUSY入力はいずれもLOWのままで、SPI転送の失敗とは別に扱った。
GPIO割当はRESET_N=1、WAKE=2、IRQ=3、CS=4、BUSY=5、SCK=7、MISO=8、MOSI=9。
SPI診断終了時はRESET_N LOW。7.522 Mbit/sは無線速度ではない。

## 無線試験と修正

最初にPR #24のFWを実機実行すると、`mmhalow_init()`が仮インターフェースを起動した後の
`mmwlan_set_channel_list()`がstatus=3で拒否された。AP開始前にFAILで終了し、STAは起動しなかった。

以下を修正した同一ソースのAP/STAで、2組をそれぞれ試験した。

1. SDKのnetif初期化後に仮インターフェースをshutdownし、単一チャンネルを設定して再bootする。
2. RF有効時のWio-WM6180向けに、transportのBUSY判定を常時trueとしてホスト側のsleepを抑止する。
   固定SDKの省電力無効shimはfalseを返し、transportがSPI IRQを無効化した後、未配線のBUSY IRQを
   待つ経路がある。基板のWAKEはpull-upで常時起床条件になるため、受信IRQを維持する。
   これはsleep禁止のソフトウェア対策であり、物理BUSYがHIGHになったという測定結果ではない。
   RF無効のpreflightでは元のHAL関数を使用する。SDK管理ファイルは変更していない。
3. 独自の明示的VIF送信処理に、lwIP WLANが呼ぶ`transmit_wrap`コールバックも登録する。
   前の実装では直接送信コールバックだけで、この入口が欠けていた。

複数の対策を合わせた後の成功であり、それぞれ単独の効果を実機で分離比較してはいない。
起動時にtransportの`Address base set failed`警告と未知TLV警告は残るが、その後FW起動・通信・終了まで完了した。
最終ソースはAP/STAのRF有効構成と、RF無効の既定AP preflight構成でビルド・リンク成功。

| 組合せ | UDP送信/一致応答 | 平均RTT | 最大RTT | 探索時RSSI |
|---|---:|---:|---:|---:|
| COM4 AP ↔ COM5 STA | 20/20 | 25.628 ms | 180.293 ms | −39 dBm |
| COM4 AP ↔ COM6 STA | 20/20 | 27.806 ms | 211.689 ms | −28 dBm |

各STAのnonce・sequence完全一致の20応答とPASS、APの20返信とPASSを確認。
最初の往復を含む値で、INFO診断ログを有効にした短時間のベンチ試験。RSSIはスキャン時の値。
長時間安定性、距離、最大スループット、DHCP、再接続、3台同時動作は未検証。

## LED・終了状態

GPIO21の状態表示入りFWを3台へ書込み済み。
接続待ちは低速点滅、認証後は点灯、UDP応答時は短く消灯、試験成功後は2回点滅を繰り返す。
GPIO設定と通信イベントはログで確認し、実際の発光の目視確認はユーザー側で未報告。
各試験終了時に無線をshutdownした。電源を入れ直すとRF試験が再開する。

APは最大STA数1、COM5/COM6は同じ固定IPなので、次回もSTAは片方ずつ試験する。
3台を同時に通信させる場合は、STAごとのIPとAP許容台数、受信側の検証条件の変更が必要。

## 証跡

- SPI: [COM4](logs/2026-09-19-halow-spi-com4.log)、[COM5](logs/2026-09-19-halow-spi-com5.log)、[COM6](logs/2026-09-19-halow-spi-com6.log)
- 初回の設定順序エラー: [AP](logs/2026-09-19-halow-attempt1-ap.log)
- 1組目: [AP](logs/2026-09-19-halow-attempt2-ap.log)、[COM5 STA](logs/2026-09-19-halow-attempt2-sta.log)
- 2組目: [AP](logs/2026-09-19-halow-attempt3-ap.log)、[COM6 STA](logs/2026-09-19-halow-attempt3-sta.log)

無線ログは`RW_LINK_`アプリ測定行を出現順に抽出。SPIログはANSI・改行・空白を正規化。
全ドライバーログ、バックアップ、書込み・ビルドログ、SHA256一覧は
`.private/halow-link-test-20260919/`に保存。COM6のSPI生ログは`.private/device-status-20260919/`。
