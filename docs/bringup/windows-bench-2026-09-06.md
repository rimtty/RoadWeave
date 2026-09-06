# Windows ESP32ベンチ環境・実測（2026-09-06）

同日追記: 環境整備後に[Windows・実機Wi-Fi/Opus UDP echo試験](windows-wifi-2026-09-06.md)を実施した。
以下の「無線未測定」は初回の環境整備完了時点の記録。

Repo `03d92e372b1a0aa3aa6683c4bcbf86769ab9a26d`のfirmwareを、native Windows PowerShellでビルド・USB書き込み・計測。
測定対象のCコードと`sdkconfig.defaults`は変更していない。今回の変更はホストツールとドキュメント。

## 導入環境

| 項目 | 確認値 |
|---|---|
| ESP-IDF | v5.4.4 / `296b6eab9445fd720e71aecab961e2d3fbca9944` |
| IDF path | `D:\Espressif\esp-idf-v5.4.4` |
| IDF_TOOLS_PATH | `D:\Espressif\tools` |
| 専用Python | 3.12.13 / `D:\Espressif\tools\python_env\idf5.4_py3.12_env\Scripts\python.exe` |
| CMake / Ninja | 3.30.2 / 1.12.1（IDF tools管理） |
| esptool / pyserial | 4.12.0 / 3.5 |
| libopus | v1.5.2 / `ddbe48383984d56acd9e1ab6a090c54ca6b735a6` |
| USB | `COM4`, Espressif `VID_303A/PID_1001`, USB Serial/JTAG |
| ESP32 | ESP32-S3 QFN56 revision 0.2, 240 MHz |
| Flash / PSRAM | 8 MiB / 8 MiB Octal 80 MHz |

Windows標準のUSB serial driverで接続できた。WSL、追加USB driver、システムPythonの変更は不要だった。
環境起動スクリプトはWindows PowerShell 5.1とPowerShell 7の両方でversion checkを通過した。
Python本体は`D:\Espressif\python\cpython-3.12.13-windows-x86_64-none`に分離している。

## 次回の起動・Opus再計測

```powershell
Set-Location D:\RoadWeave
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
. .\firmware\scripts\enter-idf.ps1
.\firmware\components\opus\fetch-upstream.ps1
python -m serial.tools.list_ports -v
idf.py -C firmware/experiments/opus_bench -D IDF_TARGET=esp32s3 build
idf.py -C firmware/experiments/opus_bench -p COM4 flash
python tools/serial_capture.py --port COM4 --reset --seconds 90 --until OPUS_BENCH_DONE --output .private/windows-bench/opus.log
```

`serial_capture.py`は有限時間でserialを閉じる。`--until`指定時はマーカーがないタイムアウトを失敗（exit 1）にする。
対話monitorは`idf.py -C firmware/experiments/opus_bench -p COM4 monitor`、終了はCtrl+]。
`set-target`は既存configを再生成するため、SSIDなどの設定後は繰り返さず`build`を使う。

## Opus実測結果

全16条件で`=== OPUS_BENCH_DONE ===`まで完走。16 kHz mono、合成信号3秒/条件、fixed-point、内部RAMにcodec state。
以下のCPU比率はESP32-S3の1 coreに対する値で、Windows CPUの使用率ではない。

| bitrate / frame / complexity | decoder数 | encode平均 | decode平均/stream | 合計CPU | 内部heap使用量 |
|---|---:|---:|---:|---:|---:|
| 12 kbps / 20 ms / c0 | 1 | 3.727 ms | 0.860 ms | 22.9% | 42,504 B |
| 12 kbps / 20 ms / c3 | 1 | 7.426 ms | 1.048 ms | 42.4% | 42,504 B |
| 12 kbps / 20 ms / c5 | 1 | 10.416 ms | 1.060 ms | 57.4% | 42,504 B |
| 12 kbps / 40 ms / c0 | 1 | 7.067 ms | 1.262 ms | 20.8% | 42,504 B |
| 12 kbps / 20 ms / c0 | 6 | 3.727 ms | 0.600 ms | 36.6% | 132,124 B |
| 12 kbps / 20 ms / c3 | 6 | 7.424 ms | 0.767 ms | 60.1% | 132,124 B |

既存macOS計測の12 kbps / 20 ms / c0（23.0%）と整合する。
encoder stateは24,548 B、decoder stateは17,800 B。
実行前後のfree internal heapは354,483 → 354,267 B（216 B減少）。この1回のログだけではリーク有無を断定しない。
全条件とboot logは[Opus実機ログ](logs/2026-09-06-windows-opus.log)。

## Wi-Fi / UDPの準備と確認範囲

`firmware/experiments/voice_udp`はADPCM・Opus 12 kbps / c0の両構成をWindowsでビルド成功。
SSID空欄のままだと`app_main()`の早期returnにより通信処理が最適化で除去されるため、
ビルド検証だけにはダミーSSID `ROADWEAVE_BUILD_ONLY`を使う。これらのバイナリは実機に書き込まない。
検証用config/buildは`.private/voice-adpcm/`と`.private/voice-opus/`に置き、通常の`voice_udp/sdkconfig`のSSIDは空のまま。

| 構成 | app binary | 1 MiB app partitionの残量 |
|---|---:|---:|
| ADPCM | `0xb8cd0` bytes（約739 KiB） | 28% |
| Opus 12 kbps / c0 | `0xe55e0` bytes（約917 KiB） | 10% |

既存コードの未使用変数`s_dec_stream`にwarningがあるが、コンパイル・リンク・partition size checkは成功。

検証用configがあるこのPCでの再ビルド:

```powershell
idf.py -C firmware/experiments/voice_udp -B D:/RoadWeave/.private/voice-adpcm/build -D SDKCONFIG=D:/RoadWeave/.private/voice-adpcm/sdkconfig build
idf.py -C firmware/experiments/voice_udp -B D:/RoadWeave/.private/voice-opus/build -D SDKCONFIG=D:/RoadWeave/.private/voice-opus/sdkconfig build
```

ホスト側は次でcodec/header selftestと実際のlocalhost UDP送受信、無受信時の時間終了、WAV確定を確認できる。

```powershell
python -B -m unittest discover -s tools -p test_rwp_peer.py -v
```

4 tests PASS。echoはADPCM/Opusどちらのdatagramもそのまま返す。WAV record/sendはADPCMのみ。

今回デバイスはルーター上に置かれているが、2.4 GHzアンテナは未装着との申告。
無線越しのRTT、packet loss、mouth-to-earは未測定。Wi-Fi接続先も未設定（`RW_WIFI_SSID`は空）。
localhostテストはWindows peerの動作確認であり、実機Wi-Fi・LAN到達性・Firewall通過の証明ではない。

アンテナ装着後の手順:

1. `idf.py -C firmware/experiments/voice_udp menuconfig`の「RoadWeave voice_udp」で2.4 GHz SSID/password、WindowsのLAN IPv4を設定する。今回の有線LAN IPv4は`10.32.1.69`（再確認すること）。Windows自体は有線でもよい。APの端末分離がない到達可能なLANを使う。
2. codecを選ぶ。Opusは12,000 bps、complexity 0。echo試験は`COORDINATOR=y`, `TX_ALWAYS=y`, `PLAY_ECHO=y`, `GROUP_BROADCAST=n`。
3. WindowsのUDP 5004受信を必要な範囲だけ許可する。下の例を参照。
4. Windows側で`python tools/rwp_peer.py echo --bind 10.32.1.69 --duration 90`を起動する。
5. 別のESP-IDF PowerShellで`idf.py -C firmware/experiments/voice_udp -p COM4 flash`、続けて`python tools/serial_capture.py --port COM4 --reset --seconds 75 --output .private/windows-bench/voice.log`。
6. 起動直後の区間を分けて60秒以上の1秒窓RTT、mouth-to-ear、gap/underrun、codec CPUを記録する。表示されるmouth-to-earはソフトウェアの再生呼び出しまでで、実音の遅延ではない。mic/ampの配線・音質は別途確認が必要。
7. peerの時間終了だけではESP32の送信は止まらない。試験後は下記smoke firmwareへ戻す。

Windows Firewallの例（管理者PowerShell。実際のnode IPへ置換してから実行）:

```powershell
$nodeIp = 'ESP32の実際のIPv4'
New-NetFirewallRule -Name RoadWeave-RWP-UDP-5004 -DisplayName 'RoadWeave RWP UDP 5004' -Direction Inbound -Action Allow -Protocol UDP -LocalPort 5004 -RemoteAddress $nodeIp -Program 'D:\Espressif\tools\python_env\idf5.4_py3.12_env\Scripts\python.exe' -Profile Any
# 試験終了後、不要なら削除:
Remove-NetFirewallRule -Name RoadWeave-RWP-UDP-5004
```

今回Firewallルールは作成していない。現在の有線LANはPublic profileのため、Privateだけのルールでは適用されない。
SSID/passwordはGit管理外の`sdkconfig`へ入力し、build成果物や録音も`.private/`などGit管理外へ保存する。

## 終了時の実機状態

```powershell
idf.py -C firmware -D IDF_TARGET=esp32s3 build
idf.py -C firmware -p COM4 flash
python tools/serial_capture.py --port COM4 --reset --seconds 15 --until HALOW_TX=DISABLED --output .private/windows-bench/windows-smoke.log
```

Windowsでビルドしたsmoke firmwareへ書き戻し、`P0A_XIAO_SMOKE=PASS`と`HALOW_TX=DISABLED`を確認済み。
このfirmwareは内蔵Wi-Fiも初期化しない。[smoke実機ログ](logs/2026-09-06-windows-smoke.log)。

## 新規Windows環境への導入メモ

公式の[Windowsセットアップ手順](https://docs.espressif.com/projects/esp-idf/en/v5.4.4/esp32s3/get-started/windows-setup.html)とRepoの`firmware/toolchain.lock`を参照。
今回のPCでは既存Git/uvを使い、公式IDF toolsスクリプトで依存ツールを取得した。インストール先は空白なしの短いパスにする。

```powershell
New-Item -ItemType Directory -Force D:\Espressif | Out-Null
uv python install 3.12.13 --install-dir D:\Espressif\python --no-bin --no-registry
git -c core.autocrlf=false clone --branch v5.4.4 --depth 1 --shallow-submodules --recursive https://github.com/espressif/esp-idf.git D:\Espressif\esp-idf-v5.4.4
$env:IDF_TOOLS_PATH = 'D:\Espressif\tools'
$idfBootstrapPython = 'D:\Espressif\python\cpython-3.12.13-windows-x86_64-none\python.exe'
& $idfBootstrapPython D:\Espressif\esp-idf-v5.4.4\tools\idf_tools.py install --targets esp32s3
& $idfBootstrapPython D:\Espressif\esp-idf-v5.4.4\tools\idf_tools.py install cmake ninja idf-exe
& $idfBootstrapPython D:\Espressif\esp-idf-v5.4.4\tools\idf_tools.py install-python-env
. .\firmware\scripts\enter-idf.ps1
.\firmware\components\opus\fetch-upstream.ps1
```

各コマンドの成功を確認して次へ進む。既にclone済みの場合は再cloneせず、固定tagとsubmodule状態を確認する。
ローカルの導入・build・flash診断ログは`.private/windows-bench/`に保存している。
