# opus_bench

ESP32-S3 上で Opus encode/decode の CPU 時間と heap を計測する（Issue #14）。

```bash
firmware/components/opus/fetch-upstream.sh
cd firmware/experiments/opus_bench
idf.py set-target esp32s3
idf.py -p /dev/cu.usbmodemXXXX flash monitor
```

`=== OPUS_BENCH_DONE ===` まで約 30 秒。結果は [docs/bringup/opus-bench-2026-09-05.md](../../../docs/bringup/opus-bench-2026-09-05.md)。
