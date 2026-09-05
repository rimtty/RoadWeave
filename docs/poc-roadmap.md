# PoCロードマップ

更新日: 2026-09-02

各phaseは機能一覧ではなく、次へ進むための測定gateを持つ。

## P0-A: HaLow host bring-up

構成: 3 x Seeed Studio XIAO ESP32S3 + 3 x Wio-WM6180、audioなし。2台をAP/STA、1台を交換診断・cross-checkに使う。

実装:

- ESP-IDF `v5.4.4`とMorse Micro component `2.11.2-esp32-2`を固定
- XIAO単体でUSB Serial/JTAG、8 MiB flash、8 MiB PSRAMを確認
- 公式`sdkconfig.defaults.seeed_xiao_esp32s3-seeed_xiao_mm6108` profileを使用
- WM6180/FGH100M-Hの`bcf_fgh100mhaamd.bin`とfirmware整合を確認
- 3組でporting assistant
- AP/STA、UDP echo、RSSI、再接続

Exit criteria:

- 3台すべてでXIAO単体smoke PASS
- 3組すべてでporting assistant全項目PASS
- 2台でcold boot 50回、driver初期化成功100%
- 8時間連続UDPでpanic/memory leakなし
- AP/STA両方の設定と復帰手順を再現可能
- WM6180のidle/RX/TX currentとRF test条件を記録

Safety gate: FGH100M-H/WM6180は902–928 MHz系のため、日本国内の通常空間へ送信しない。RF端子を開放したまま送信せず、conducted/shielded testまたは適法な地域・設備を使う。

Stop condition: 公式profileで3組のporting assistantを再現できない、RF試験方法を確保できない、またはBCF/FWのlicense/供給条件を確認できない場合はAP/STAへ進まず、Morse Micro/Seeedへ確認する。

### P0-Aから製品main lineへの移行

WM6180でhost/networkを成立させた後、HC01 V2 carrierで同じtest suiteを再実行する。HC01 V2の3.3 V/5 V rail、27 dBm PA、BCF/FW、antenna pathは別gateであり、WM6180のPASSを代用しない。日本向けはFGH100M-JとMRF61_Aを比較する。

## P0-B: 2-node breadboard IP-PTT

構成: 2 x ESP32-S3 + validated HaLow reference + I2S mic + I2S amp + speaker + wired PTT。最初はWM6180、製品main lineではHC01 V2へ同じvoice testを移植する。

実装:

- 16 kHz mono PCM
- IMA-ADPCM 64 kbps、20 ms packet
- UDP sequence、PTT_START/END、20〜40 ms jitter buffer
- TX中speaker hard mute

Exit criteria:

- quiet benchでend-to-end p95 <=150 ms
- 30分会話でPTT stuck 0、audio underrun <1/10 min
- 1/2 MHz、packet size、MCSごとのloss/airtime/power表
- 電源挿抜、packet loss、Node再起動から自動復帰

## P1: Group IP-PTT

- **6台**（製品定義 v2: 通常 6〜8 台）、Group ID/User ID、active speaker。追加ハードは Issue #20
- Coordinator AP + STA
- floor request/grant/lease、all-group PTT
- 2.8in タッチ表示（LVGL、`experiments/ui_lvgl`）に`RX: USER`、link、battery、convoy

Exit criteria:

- 6台で同時PTT競合を安全に解決（BUSY_WAIT、host simulator で先行検証済み）
- 4時間の移動/bench attenuation試験で制御state不整合なし
- group参加/離脱が音声streamを壊さない

## P2: Vehicle PoC

- 3000 mAh battery、power path、外部車載antenna
- 2台以上で高速/市街/峠を安全な同乗評価体制で測定
- noise対策、compressor/limiter、night UI
- 2 MHz normal / 1 MHz long-range比較

Exit criteria:

- typical profile 12時間以上
- 車両電源・battery双方でbrownoutなし
- route別RSSI/loss/latency/GPS log
- 操作が運転注意を不当に奪わないことを同乗者が評価

## P3: Convoy GPS

- 1 Hz position beacon、age/quality
- Convoy / heading-up Radar / breadcrumb Route
- route上distance projection
- 8台simulation + 4台実機

Exit criteria:

- 位置の古さを誤って現在位置として表示しない
- 8 node相当でvoice p95 latencyをP1比+20 ms以内に維持
- breadcrumb memoryが上限内で循環し、長時間断片化しない

## P4: Opusと個別制御

- Opus voice 12/16 kbps、complexity 0〜1（6 decode 実測: c0 36.6% / c3 60.1%）
- individual mute、gain、targeted PTT
- leader/subgroup/private target
- application protocol versioning

Exit criteria:

- 8台、1 active speakerでCPU/RAM/qualityを測定
- 2 simultaneous speakers を正式仕様に（無線帯域の上限。MCU は 6 decode 可）
- target外へprivate audio payloadを送らない設計をpacket captureで確認

## P5: Recording

- microSDへOpus frame + timestamp + sender ID + GPS event
- ring buffer、graceful close、recovery index
- visible REC indicator、参加者同意flow

Exit criteria:

- power cut後に直前file以外を失わない
- 8時間logの再生・sender filter・GPS同期
- recordingでvoice latency/lossが悪化しない

## P6: Japan line

- MRF61_A carrier/EVKで同じvoice/position protocol
- 国内channel、power、Duty、指定antenna条件
- 技適表示、最終製品要件、EMC/安全を専門家と確認

Exit criteria:

- HC01主系との差分BOM/firmware/test report
- 法令上許容される設定をfactory lockできる
- RF airtime実測が適用制限内

## P7: PMTiles from microSD

- 地域限定PMTiles archive
- local file range reader、tile cache
- raster方式とpreprocessed vector方式を比較
- OSM attribution表示/同梱

Exit criteria:

- pan/zoomではなく走行追従時のframe timeと消費電力を評価
- voice deadlineを阻害しないI/O priority
- SD抜去/破損時もconvoy/breadcrumbへ自動fallback

## Rev.A PCBへ進む条件

P0-B完了に加え、HC01 BCF/FW供給、電源peak、SPI signal integrity、audio noiseの4点が再現できること。LCDやGPSの完成を待つ必要はない。
