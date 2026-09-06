# voice_udp

HaLow 到着前に、XIAO の内蔵 2.4 GHz Wi-Fi を代役にして音声パイプラインを end-to-end で通す実験。
I2S mic → HPF → IMA-ADPCM → RWP/0.1 VOICE → UDP → (Mac) → UDP → jitter buffer → decode → I2S amp。
Morse component も lwIP の netif を提供するので、HaLow 到着後は socket 部分をそのまま流用できる。

## 使い方

**先に XIAO の U.FL に 2.4 GHz アンテナを付ける。** アンテナなしでは数十秒でリンクが崩れ、PA にも負荷がかかる（[2026-09-06 計測](../../../docs/bringup/voice-udp-2026-09-06.md)）。

```bash
cd firmware/experiments/voice_udp
idf.py set-target esp32s3
idf.py menuconfig      # RoadWeave voice_udp: Wi-Fi SSID/pass, peer IP (Mac), pins
idf.py -p /dev/cu.usbmodemXXXX flash monitor
```

Mac 側（別ターミナル）:

```bash
python3 tools/rwp_peer.py echo            # 往復させて RTT と mouth-to-ear を node 側に表示
python3 tools/rwp_peer.py record mic.wav  # mic の音を Mac に WAV で保存（amp 不要でマイク品質を確認）
python3 tools/rwp_peer.py send voice.wav --to <node ip>   # Mac から node の speaker へ
```

node の 1 秒ごとの出力例:

```
tx 50 rx 50 bad 0 | echo 50 rtt avg 6 max 14 ms | mouth-to-ear ~88 ms | jb depth 40 played 48 gap 0 late 0 underrun 1
```

`mouth-to-ear` は capture → encode → UDP 往復 → jitter buffer → decode → I2S 書き込みまで。
スピーカーの実音までは I2S DMA 分（約 20〜40 ms）を足す。SSID/パスワードは `sdkconfig`（git 管理外）にだけ入る。

## Windows PowerShell

Repoルートから`. .\firmware\scripts\enter-idf.ps1`で専用環境を起動できる。
`RW_PEER_IP`にはWindowsのLAN IPv4を設定する。Windowsは有線LANでもよく、ESP32の2.4 GHz Wi-Fiから到達できることが条件。

```powershell
idf.py -C firmware/experiments/voice_udp menuconfig
idf.py -C firmware/experiments/voice_udp -p COM4 flash
```

別ターミナルで環境を起動し、peerを動かす:

```powershell
python tools/rwp_peer.py echo --duration 90
```

peer実行中に元のターミナルでログを採る:

```powershell
python tools/serial_capture.py --port COM4 --reset --seconds 75 --output .private/windows-bench/voice.log
```

`--duration`はecho/recordを指定秒で終了する。無受信でも終了し、recordのWAV headerを確定する。
`--bind <Windows LAN IPv4>`で待受interfaceを指定できる。既定は全IPv4 interface。
echoはADPCM/Opusの両方に対応し、record/sendのcodecはADPCMのみ。
`--duration`で終了するのはPC側peerだけで、ESP32の送信は停止しない。試験後は`firmware/`のsmoke firmwareへ書き戻す。

Windows Firewallの限定ルール、アンテナ条件、ビルド検証結果は[Windowsベンチ手順](../../../docs/bringup/windows-bench-2026-09-06.md)を参照。

## モード（menuconfig "RoadWeave voice_udp"）

| 設定 | 意味 |
|---|---|
| `RW_COORDINATOR=y` | この node が Group Coordinator（floor 管理）。Mac echo のベンチでは y のまま（自分で自分に grant） |
| `RW_COORDINATOR=n` + `RW_COORD_IP` | 別の node が coordinator。REQUEST/RENEW/END を UDP で送り、GRANT/DENY を待つ |
| `RW_GROUP_BROADCAST=y` | voice を IPv4 broadcast へ送る（同じ AP 上の 2 台以上） |
| `RW_TX_ALWAYS=y` | PTT を押しっぱなしとみなす（ベンチ用。120 s ごとに END → 再要求が入る） |
| `RW_PLAY_ECHO=y` | 自分の sender_id のフレームも再生（Mac echo で自分の声を聞く） |

2 台構成の例: node A `COORDINATOR=y, BROADCAST=y`、node B `COORDINATOR=n, COORD_IP=<A の IP>, BROADCAST=y`。

実装済み: floor control（`components/rwp/floor.h`、BUSY_WAIT 再要求含む）、TX 中の speaker hard mute、
簡易 PLC（gap で直前フレームを -6 dB ずつ最大 3 frame 繰り返し）。1 秒ごとの行に floor 状態と grant/busy/fail 数が出る。

## 未実装（P0-B 本実装で入れる）

- targeted PTT（NODE / SUBGROUP 宛）と application 暗号化
- PC側のOpus WAV record/send（node側の`RW_CODEC_OPUS`は実装済み）
