# Windows・実機Wi-Fi/Opus UDP echo試験（2026-09-06）

指定された2.4 GHz APへの接続とWindowsとのUDP往復通信を確認した。
アンテナ未装着、ESP32をルーター上に置いた条件で実施。接続は維持できたが、RTTは秒単位になり、今回の配置で低遅延音声の性能は得られなかった。

## 条件

- Repo基準: `45a6128`。`firmware/experiments/voice_udp`のCコードは変更せず使用。
- Windows native ESP-IDF v5.4.4、XIAO ESP32-S3 240 MHz、COM4。
- Opus fixed-point 12 kbps、16 kHz mono、20 ms/frame、complexity 0、coordinator自身へのgrant、常時TX、echo再生。
- ESP32内蔵2.4 GHz Wi-Fi → AP → Windows有線LAN → `tools/rwp_peer.py echo` → ESP32。
- AP接続時: channel 4、WPA2-PSK、RSSI **−88 dBm**。これは接続時の1回の表示値で、試験全体のRSSI分布ではない。
- Windows peerはPython **3.13**で実行。既存のPublic profile受信許可を利用し、Firewall変更なし。ビルド・USBログ採取はESP-IDF専用Python 3.12。
- パスワードはWindows Terminalの非表示入力でGit管理外のconfigへ保存。公開ログではSSIDとMAC/BSSIDを伏せている。
- USBログ採取75秒（boot/接続時間を含む）、接続後の1秒統計行は69個。IP取得はbootから約4.96秒。
- mic/ampの配線・実音・音質は確認していない。ここでは既存コードによるcodec/UDP/jitter buffer経路を測定した。

## 結果

**RTT平均・mouth-to-ear平均の中央値/p95は、69個の「1秒窓平均値」を並べた統計。個々のpacket/frameのp95ではない。**
p95はnearest-rank（昇順の`ceil(0.95 × N)`番目）で算出。

| 指標 | 中央値 | p95 | 最大 |
|---|---:|---:|---:|
| 1秒窓のRTT平均 | 1,280 ms | 1,820 ms | 2,295 ms |
| 1秒窓のmouth-to-ear平均 | 1,279 ms | 1,984 ms | 2,453 ms |
| audio処理負荷（firmware表示、1 core比） | 19% | 21% | 22% |

- すべての窓の`rtt max`の最大: **2,604 ms**。
- 最後の60窓でもRTT平均値の中央値1,356 ms、mouth-to-ear平均値の中央値1,413.5 ms。起動直後だけの遅延ではない。
- 最終serial統計: TX 2,232、RX 1,867、bad 0、played 1,409、gap 391、late 0、underrun 88。
- jitter buffer深さは最初の表示60 msから80 msに達し、その後80 msを維持。
- floorは69窓すべてTALK、grant 1、busy/fail 0。取得ログにWi-Fi切断、panic、再起動はない。

TXは`sendto()`成功回数であり、音声sequenceは成功/失敗にかかわらず増える。さらにTX/RXは採取境界で未帰還packetを含むため、
単純な`1 - RX/TX`を無線packet loss率として扱わない。PC側ログの`lost`もsequence欠番数であり、送信失敗と経路上の欠落を分離できない。
PC側peerはflash前からsmoke復帰まで動作しているため、その累積カウンタの区間は75秒のserial採取と完全には一致しない。

`mouth-to-ear`表示はcapture時刻からソフトウェアの再生呼び出しまで。スピーカー実音までの遅延ではない。
audio処理負荷は既存firmwareが計測する処理経過時間で、ハードウェアCPU profilerの値ではない。

## 判断

WindowsでRepoのWi-Fi音声経路をビルド・書き込み・計測できることは実機確認できた。
ルーター直上でも接続時RSSIは−88 dBmであり、今回の配置は十分な受信強度を得られていなかった。
弱いリンクが遅延・欠落に寄与している可能性が高いが、アンテナ装着ありの対照測定をしていないため、原因をアンテナだけに断定しない。
次の比較条件は2.4 GHzアンテナを装着した同じcodec・同じ75秒試験。

## 再実行

このPCの接続設定は`.private/wifi-live-20260906/sdkconfig`（Git管理外）。他のPCではmenuconfigから設定する。

```powershell
Set-Location D:\RoadWeave
. .\firmware\scripts\enter-idf.ps1
idf.py -C firmware/experiments/voice_udp -B D:/RoadWeave/.private/voice-opus/build -D SDKCONFIG=D:/RoadWeave/.private/wifi-live-20260906/sdkconfig build
```

Windows peerを別ターミナルで開始してからflashし、serialを採取する。このPCで使用したpeerコマンド:

```powershell
py -3.13 tools/rwp_peer.py echo --bind 10.32.1.69 --duration 180
```

ESP-IDF側ターミナル:

```powershell
idf.py -C firmware/experiments/voice_udp -B D:/RoadWeave/.private/voice-opus/build -D SDKCONFIG=D:/RoadWeave/.private/wifi-live-20260906/sdkconfig -p COM4 flash
python tools/serial_capture.py --port COM4 --reset --seconds 75 --output .private/wifi-live-20260906/voice.log
idf.py -C firmware -p COM4 flash
python tools/serial_capture.py --port COM4 --reset --seconds 15 --until HALOW_TX=DISABLED --output .private/wifi-live-20260906/restored-smoke.log
```

実行時はPowerShellの`finally`でsmoke firmware復帰を行った。最終状態は`P0A_XIAO_SMOKE=PASS`、`HALOW_TX=DISABLED`。
smoke firmwareは内蔵Wi-Fiも初期化しない。PCのUDP peerも終了済み。

## 記録

- [実機serialログ](logs/2026-09-06-windows-wifi-opus.log)
- [Windows UDP peerログ](logs/2026-09-06-windows-wifi-peer.log)
- [smoke復帰ログ](logs/2026-09-06-windows-wifi-restored.log)
- [環境準備・Opus単体計測](windows-bench-2026-09-06.md)

ログは改行と末尾空白を整え、SSID・MAC/BSSIDを伏せた。パスワードを含まないことを保存前に検証した。
