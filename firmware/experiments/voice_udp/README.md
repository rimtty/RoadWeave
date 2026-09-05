# voice_udp

HaLow 到着前に、XIAO の内蔵 2.4 GHz Wi-Fi を代役にして音声パイプラインを end-to-end で通す実験。
I2S mic → HPF → IMA-ADPCM → RWP/0.1 VOICE → UDP → (Mac) → UDP → jitter buffer → decode → I2S amp。
Morse component も lwIP の netif を提供するので、HaLow 到着後は socket 部分をそのまま流用できる。

## 使い方

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

## 未実装（P0-B 本実装で入れる）

- floor control（`components/rwp/floor.h`）との結線。今は TX_ALWAYS か PTT 直結。
- PLC（gap 時は無音）。
- 2 node 間の直接通信（今は Mac 経由の echo）。
