# ui_lvgl

2.8 インチ ILI9341（SPI）+ XPT2046 タッチ + LVGL 9 の UI 先行実装（`docs/display-selection.md`）。
GROUP（誰が話しているか）/ RADAR（heading-up）/ CONVOY（前後距離）の 3 画面と、下段の MUTE / PTT（長押し）/ NEXT ボタン。
`RW_UI_SIMULATE=y` なら GPS も無線もなしで 3 台の隊列が動くので、ディスプレイが届けばそのまま見た目を詰められる。

```bash
cd firmware/experiments/ui_lvgl
idf.py set-target esp32s3
idf.py menuconfig    # RoadWeave UI: LCD/touch pins, SPI MHz
idf.py -p /dev/cu.usbmodemXXXX flash monitor
```

既定ピンは WM6180 と同じ側面ピン（SCLK=D8/GPIO7、MOSI=D10/GPIO9、MISO=D9/GPIO8、CS=D3/GPIO4、DC=D2/GPIO3、RST=D1/GPIO2、BL=D0/GPIO1、T_CS=D4/GPIO5）。
**HaLow モジュールを載せた XIAO では使えない**（ピン競合）。UI 単体の検証用。Rev.A では LCD を別 SPI（AUX_SPI）に置く。

画面の設計ルール: 一目で読める大きな文字、深いメニューなし、走行中はボタン 3 つだけ。位置の古い peer は `?`（3 s）/ `x`（10 s）で示し、
距離を確定表示しない（`docs/gps-and-maps.md` §2）。
