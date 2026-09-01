# GPS隊列・breadcrumb map・PMTiles構想

更新日: 2026-09-02

## 1. 設計思想

1.47 inch画面で必要なのは一般的なnavigation mapより、「誰が前後どれだけ離れたか」「次の曲がりの向こうに誰がいるか」である。そこで地図dataなしで成立する3画面を先に作る。

### Convoy

```text
LOTUS     +120 m
NA        +340 m
YOU
ND        -620 m
SPREAD     980 m
```

### Heading-up Radar

```text
       A 120m
          *
    B *       * D
         YOU
      scale 500m
```

### Breadcrumb Route

```text
---- A
     \
      * B
      |
     YOU
      |
      D
```

「走った軌跡がその場で地図になる」をRoadWeave固有の中心体験とする。

## 2. Position beacon draft

1 Hzを初期値とする。

```text
protocol_version
group_id, sender_id
sequence
gnss_utc_ms
latitude_e7, longitude_e7
altitude_dm
speed_cms
heading_cdeg
horizontal_accuracy_cm
fix_type, satellites
```

受信時刻とageをpeer stateに持つ。3秒超はdim、10秒超はstaleとしてdistance/rankを確定表示しない。測位なしのpeerを(0,0)へ描かない。

## 3. Local coordinate

group開始地点または現在のroute anchorを原点にWGS84緯度経度をlocal ENU（east/north/up、meter）へ変換する。短距離表示ではfloatで足りるが、原点から離れたらrebaseする。

```text
struct LocalPoint {
    float east_m;
    float north_m;
};
```

breadcrumbは距離/方位変化で追加し、ring buffer化する。例として2D float 8 byte x 512点 = 4 KiB。時刻やsourceを持つ保存形式は別bufferにし、Douglas-Peucker等の簡略化は走行loopより低priorityで実行する。

## 4. Route-relative distance

直線距離だけでは峠道を横切って短く見える。breadcrumb polylineへ各車をprojectionし、polyline上の累積距離差を計算する。

```text
peer fix -> nearest valid route segment -> along-route position s_peer
self fix -> along-route position s_self
display distance = s_peer - s_self
```

誤projection対策:

- routeから一定距離以上なら直線距離へfallback
- 交差/折返しではheadingと直近segment historyを加味
- route version/leader sourceを表示せず内部管理
- GNSS jumpをspeed/accuracy gateで除外

## 5. GPX bridge

PMTilesの前に、smartphone/USBから事前GPX polylineを転送する段階を置く。数十KiB級で道路形状と予定routeを表示でき、tile renderer不要。planned routeとlive breadcrumbを色/線種で分ける。

## 6. PMTiles from microSD

PMTilesはtile pyramidを単一fileにしたread-only archiveで、必要なtile/metadataだけをrange readする設計である。[PMTiles concepts](https://docs.protomaps.com/pmtiles/)

microSD上ではHTTPの代わりに同じoffset/length abstractionをlocal `seek + read`へ実装する。

```text
MapView z/x/y
  -> PMTiles directory lookup
  -> local range reader
  -> compressed tile
  -> decode
  -> style/render
  -> LCD tile cache
```

難所はarchive readではなく、その後のdecode/renderである。

- vector MVT: protobuf、geometry、font/icon、style engineが重い
- raster tile: 実装は軽いがstorageが大きくzoom/style固定
- 172x320: 一般mapの情報量が多すぎるため専用styleが必要
- microSD I/Oがvoice deadlineを阻害してはならない

## 7. 実装段階

| Stage | Data | Renderer | 目的 |
|---|---|---|---|
| M0 | none | convoy/radar | 仲間の前後・距離 |
| M1 | live breadcrumb | polyline | 走行軌跡が地図になる |
| M2 | transferred GPX | polyline | 予定route比較 |
| M3 | pre-rendered raster PMTiles | bitmap crop/blit | local range readerとcache測定 |
| M4 | preprocessed vector PMTiles | limited roads/labels | 専用styleでstorage削減 |
| M5 | richer regional map | only if proven | 製品価値と電力を再評価 |

P0〜P3でmicroSDを要求しない。SDが抜けてもvoice、GPS beacon、convoy、breadcrumbは動く。

## 8. Data preparation

- 端末でplanet fileを扱わない。都道府県/route corridor単位に切る
- zoom範囲、feature、languageをbuild時に制限
- tile archiveはread-only、更新はUSB/smartphoneからatomic replace
- archive hash、version、area、attributionをmanifestへ記録
- corrupt/unsupported archiveはmountせずsafe fallback

OpenStreetMap由来dataを使う場合はODbLとattribution要件を確認し、画面または製品内の適切な場所に `© OpenStreetMap contributors` と必要な案内を表示する。[OpenStreetMap copyright and license](https://www.openstreetmap.org/copyright)

## 9. 性能budget

地図taskはaudio/networkより低priorityにする。

暫定budget:

- map update: 2〜5 fpsで十分
- visible cache: PSRAM 1〜2 MiB以内から開始
- single range read: p95 10 ms目標
- tile decode/render: 1 frame 100 ms以内を目標
- voice underrun増分: 0
- map active時の追加平均電力: 0.2 W以内を目標

満たせない場合はvectorを無理に続けず、raster/GPX/breadcrumbへ戻す。
