# アーキテクチャ概要

更新日: 2026-09-02

## 1. 目的

RoadWeaveは車列向けのローカルIPネットワーク端末である。一般的なトランシーバーの置き換えだけでなく、同じHaLowリンク上に音声、発話者ID、GPS、車速、方位、接続状態を載せる。

設計目標:

- 携帯回線・クラウド不要
- P0は2台、量産構想は通常8台、設計上限は16台を暫定目標
- 半二重PTTのend-to-end音声遅延: 100〜150 ms以内
- 標準3000 mAh 1S LiPoで通常ツーリング15時間をストレッチ目標、12時間を最低設計目標
- 車載外部アンテナとハンディ運用の両方を許容
- 音声と位置情報はネットワーク断から自動復帰
- 日本向けRFはMRF61_Aラインで別途成立性を検証

## 2. 論理構成

```text
                         920/915 MHz Wi-Fi HaLow
                    ┌────────────────────────────┐
                    │ AP/STA group, WPA3-SAE     │
                    └──────────────┬─────────────┘
                                   │ SPI + IRQ/WAKE/BUSY
┌─────────────── RoadWeave node ───┴──────────────────────────┐
│ ESP32-S3-WROOM-1-N16R8                                     │
│                                                            │
│  audio capture -> codec -> UDP voice -> jitter buffer       │
│  group/floor control -> peer policy -> local mixer          │
│  GNSS -> position broadcast -> convoy/radar/breadcrumb UI   │
│  event log -> future recorder/storage abstraction           │
│                                                            │
│ I2S mic   I2S amp   1.47in IPS   GNSS   keys   microSD pad  │
└──────────────┬──────────────────────────────────────────────┘
               │
       USB-C / 1S LiPo / 3.3 V buck / 5 V boost
```

## 3. ハードウェア基準構成

| ブロック | Rev.A方針 | 理由 |
|---|---|---|
| MCU | ESP32-S3-WROOM-1-N16R8 | PSRAMを音声buffer、地図cache、UIに使い、16 MB flashにHaLow FW/BCFとOTA余地を確保 |
| HaLow main | HT-HC01 V2、SPI host | 27 dBm級PA/LNA内蔵。ESP32向け公式コンポーネントのSPI構成と合わせる |
| HaLow Japan | MRF61_A_FL | MM6108共通性を保ちつつ、日本向け認証モジュールを評価 |
| Mic | 16 kHz対応I2S MEMS（T5848級） | ADC/アナログプリアンプを避ける |
| Speaker | MAX98357A級 + 4 ohm 3 W | I2S DAC/Class-D一体、PoC入手性 |
| Display | 1.47 inch ST7789系 172x320 IPS | 発話者と隊列を縦画面に表示しやすい |
| GNSS | UART、1 Hz以上、PPS pad | まず位置・速度・方位。型番はPoC比較後に固定 |
| Storage | Rev.AにmicroSD socket/pad、P0はDNP可 | 録音とPMTilesを基板改版なしで試せる余地 |
| Battery | protected 1S LiPo 3000 mAh | 薄型筐体と一日運用のバランス |

## 4. ソフトウェア境界

| コンポーネント | 責務 | 他層への依存 |
|---|---|---|
| `halow_port` | MM6108起動、BCF/FW、AP/STA、RSSI、power-save | Morse Micro ESP-IDF component |
| `voice_transport` | voice/control UDP、sequence、loss統計 | socketsのみ |
| `audio_pipeline` | I2S、codec、jitter buffer、limiter | network非依存 |
| `group_service` | peer表、floor、target、mute/gain | transport abstraction |
| `position_service` | GNSS、位置broadcast、clock quality | UART/GNSS abstraction |
| `route_model` | ENU座標、breadcrumb、route projection | map renderer非依存 |
| `ui` | active speaker、group、convoy/radar/route | read-only state models |
| `storage` | settings、logs、将来のrecord/PMTiles | flash/microSD abstraction |

音声、位置、UIをHaLowドライバから直接呼ばない。HC01 V2からMRF61_Aへ切り替える際に、`halow_port`とRF設定以外をできるだけ共通化する。

## 5. ネットワーク形態

P0/P1は、1台をGroup Coordinator兼HaLow AP、他をSTAとするstarを採用する。ルーターもインターネットも不要で、APはIP接続点にすぎない。Coordinator故障時の再選出はP3以降に追加する。

```text
           CAR-02
              |
CAR-03 --- CAR-01(AP) --- CAR-04
              |
           CAR-05
```

最初から802.11sやMANETを必須にしない。車列では中継が価値を持つが、音声・電力・RFの基礎測定より先にmeshの不確実性を持ち込まない。

## 6. データフロー

### TX

`PTT -> floor request -> mic I2S -> high-pass/AGC -> codec -> voice packet -> UDP -> HaLow`

### RX

`HaLow -> UDP -> peer policy -> jitter buffer -> decode -> gain/mute -> limiter -> I2S amp`

### Position

`GNSS -> fix validation -> 1 Hz beacon -> peer table -> local ENU -> convoy/radar/route UI`

## 7. 信頼性と安全側の挙動

- PTTを離したらローカルTXを即停止し、PTT_END欠落はlease timeoutで回収する。
- link断時に音量最大・speaker pop・PTT stuckを起こさない。
- LiPo低電圧時はLCD輝度とTX出力を段階制限し、突然電断する前にログを閉じる。
- GNSS fixが古いpeerは距離を確定表示せず、ageを示す。
- 運転中UIは一目で読める表示を優先し、設定操作を制限する。
- 録音は初期状態OFF。録音中は常時表示し、地域の会話録音ルールと参加者同意を製品要件にする。

## 8. 未決事項とゲート

| ID | 未決事項 | 解消条件 |
|---|---|---|
| A-01 | HC01 V2に対応するBCF/FWの正式な組み合わせ | porting assistant全項目PASS、AP/STA両方の連続試験 |
| A-02 | ESP32-S3でOpusを何stream処理できるか | 8/12/16 kbps、1〜2同時speaker、CPU/RAM/温度実測 |
| A-03 | HC01とMRF61の共通carrier PCB可否 | pinout、電源、keepout、認証アンテナ条件比較 |
| A-04 | 通常group上限 | 8/16台でpacket loss、floor時間、GPS beacon airtime測定 |
| A-05 | PMTiles方式 | raster/vector双方のmicroSD seek、decode、frame time測定 |
