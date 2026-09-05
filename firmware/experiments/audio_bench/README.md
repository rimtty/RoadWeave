# audio_bench

P0-B の local audio loopback（Issue #4）用の実験firmware。I2S full-duplex で MEMS mic を取り込み、
level meter を表示し、MAX98357A へ折り返す。PTT を押している間は speaker を hard mute する。

配線の既定値は Issue #18 の draft（BCLK=D11/GPIO42、WS=D12/GPIO41、MIC=D5/GPIO6、SPK=D6/GPIO43、PTT=D7/GPIO44）。
`idf.py menuconfig` → "RoadWeave audio bench" で変更できる。amp がまだ無い場合は SPK_DOUT を -1 にすると capture のみになる。

```bash
cd firmware/experiments/audio_bench
idf.py set-target esp32s3
idf.py menuconfig    # pins / loopback / test tone
idf.py -p /dev/cu.usbmodemXXXX flash monitor
```

出力例（500 ms ごと）:

```
[########............] rms  -36.2 dBFS peak  1830 dc     -3 | proc avg  180 us max  220 us ( 1% of 20 ms) | rx_ovf 0 tx_udf 0 | RX
```

記録する値は [audio bench notes](../../../docs/bringup/audio-bench-notes.md) を参照。
