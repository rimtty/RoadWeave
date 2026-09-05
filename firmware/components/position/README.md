# position component

P3 の位置ビーコンと隊列表示のロジック（`docs/gps-and-maps.md` §2〜§4）。GNSS モジュール到着前に pure C で先行実装。

- `pos_beacon_t` の 28 byte wire format（group/sender は RWP header 側）。fix なしや (0,0) は `pos_beacon_valid()` が弾く
- local ENU（equirectangular、float、antimeridian 対応）、距離・方位、進行方向への射影（Convoy の +120 m / -620 m）
- peer table（16 台、newest-seq wins、3 s で dim / 10 s で stale、expire）

```bash
make -C firmware/components/position/test_host test
```

未実装: breadcrumb ring buffer と route projection（§4）。P3 で GNSS 実測と一緒に入れる。
