# audio_pipeline component

- `adpcm.[ch]`: IMA ADPCM（16 kHz mono、4 bit/sample）。20 ms block = 4 byte state header + 160 byte = 164 byte（65.6 kbps）。
  block ごとに predictor state を持つので、packet loss 後も次の block を独立に decode できる。
- `jitter.[ch]`: sequence 順の固定 slot jitter buffer。prefill、late/duplicate/gap/underrun 統計、late 頻発で深さを +1 frame、
  過剰深さが続けば -1 frame（min 20 / max 80 ms）。stream 切替で flush。

ホストテスト:

```bash
make -C firmware/components/audio_pipeline/test_host test
```
