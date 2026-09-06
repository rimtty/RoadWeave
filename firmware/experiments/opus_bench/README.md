# opus_bench

ESP32-S3 上で Opus encode/decode の CPU 時間と heap を計測する（Issue #14）。

```bash
firmware/components/opus/fetch-upstream.sh
cd firmware/experiments/opus_bench
idf.py set-target esp32s3
idf.py -p /dev/cu.usbmodemXXXX flash monitor
```

`=== OPUS_BENCH_DONE ===` まで約 30 秒。結果は [docs/bringup/opus-bench-2026-09-05.md](../../../docs/bringup/opus-bench-2026-09-05.md)。

Windows PowerShell（Repoルートから）:

```powershell
. .\firmware\scripts\enter-idf.ps1
.\firmware\components\opus\fetch-upstream.ps1
idf.py -C firmware/experiments/opus_bench -D IDF_TARGET=esp32s3 build
idf.py -C firmware/experiments/opus_bench -p COM4 flash
python tools/serial_capture.py --port COM4 --reset --seconds 90 --until OPUS_BENCH_DONE --output .private/windows-bench/opus.log
```

`COM4`は2026-09-06の実機port。[Windows実測結果](../../../docs/bringup/windows-bench-2026-09-06.md)。
