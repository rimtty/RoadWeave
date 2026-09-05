# Audio bench notes（P0-B、Issue #4/#18向け）

更新日: 2026-09-05

## I2S MEMS mic（SPH0645LM4H / INMP441）

- 3.3 V給電、SELはGNDで左channel。片channelだけ使う場合、ESP-IDF I2S std modeで`slot_mode=MONO`、`slot_mask=LEFT`。
- 出力は32 bit slotの上位18 bit（SPH0645）/ 24 bit（INMP441）。SPH0645はデータが1 BCLK遅れて出る癖があり、`std_slot_cfg.bit_shift = true`（Philips format）で受ける。値の扱いは符号付き32 bitとして受けて右shift。
- BCLK下限は約1.024 MHz。16 kHz x 32 bit x 2 slot = 1.024 MHzなので16 kHz mono運用は下限ぎりぎりで動く。不安定なら32 kHzで取り込み2:1 decimationする。
- DC offsetと低域の持ち上がりがあるため、80〜120 Hzの1次high-passを入口に置く。
- mic breakoutのGNDとampのGNDをbreadboard上で最短で結び、class-D出力線をmicから離す。

## I2S amp（MAX98357A）

- 電源2.5〜5.5 V。3.3 V給電では4 ohmで約1 W。車内向けの音量評価は5 V（USB VBUS）給電で行い、電力測定時は3.3 V/5 Vの両方を記録する。
- SD（shutdown）pinはfirmware依存にせず、外部pull-downで起動時mute。PTT TX中はSDでhard mute。
- GAIN pinのstrapで9/12/15 dB等を選ぶ。まず12 dB。
- BCLK/LRCLKはmicと共有できる（ESP32-S3 I2S full-duplex）。DINだけ別GPIO。
- 電源投入直後とstream切替時のpopは、SD解除をI2S clock安定後まで遅らせて抑える。

## PWMとaudioの干渉（Rev.A以降）

- LCD backlight PWMは20 kHz以上にする。可聴域PWMはmic/ampへノイズとして乗る。
- 5 V boostのswitching nodeとclass-D配線をmic、GNSS、HaLow RFから離す。

## P0-B測定で残すもの

- I2S underrun/overrun回数、CPU使用率、内部heap/PSRAM使用量
- mic無音時のnoise floor（RMS）、1 kHz tone loopbackのTHD目安
- 3.3 V/5 V給電それぞれのamp平均・peak電流
