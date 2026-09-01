# PoCロードマップ

更新日: 2026-09-02

各phaseは機能一覧ではなく、次へ進むための測定gateを持つ。

## P0-A: HaLow host bring-up

構成: ESP32-S3 dev board + HC01 V2 breakout/debug board、audioなし。

実装:

- Morse Micro ESP-IDF componentのversion固定
- HC01 V2のBCF/FW確認
- porting assistant
- AP/STA、UDP echo、RSSI、再接続

Exit criteria:

- 2台でboot 50回、driver初期化成功100%
- 8時間連続UDPでpanic/memory leakなし
- AP/STA両方の設定と復帰手順を再現可能
- HC01 3.3 V/5 V railのidle/TX peakを記録

Stop condition: HC01固有BCFが合法的・再現可能に入手できない場合、主系を一時停止し、Morse Micro対応評価moduleまたはMRF61_Aでsoftware pipelineを先行する。

## P0-B: 2-node breadboard IP-PTT

構成: 2 x ESP32-S3 + HC01 V2 + I2S mic + I2S amp + speaker + wired PTT。

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

- 4台、Group ID/User ID、active speaker
- Coordinator AP + STA
- floor request/grant/lease、all-group PTT
- 1.47in displayに`RX: USER`、link、battery

Exit criteria:

- 4台で同時PTT競合を安全に解決
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

- Opus voice 8/12/16 kbps、40 msを比較
- individual mute、gain、targeted PTT
- leader/subgroup/private target
- application protocol versioning

Exit criteria:

- 8台、1 active speakerでCPU/RAM/qualityを測定
- 2 simultaneous speakersは実測で余裕がある場合のみ有効化
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
