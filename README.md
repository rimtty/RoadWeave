# RoadWeave

RoadWeave は、インターネットや携帯回線がない場所でも、ツーリング中の車両どうしでグループ音声と位置情報を共有する小型端末の研究開発プロジェクトです。

製品の核は **ESP32-S3 + Wi-Fi HaLow + IP-PTT** です。最初は2台のブレッドボードで低遅延の半二重音声を成立させ、そこから「オフラインのDiscord風ボイスルーム」、指定相手へのPTT、個別ミュート・音量、音声記録、GPS隊列表示へ段階的に育てます。

## 開発ライン

| ライン | 無線モジュール | 位置づけ |
|---|---|---|
| P0 reference | Seeed Studio Wio-WM6180 / FGH100M-H / MM6108 | XIAO ESP32S3へ直接挿し、公式board profileでhost/networkを先行検証する。量産BOMの決定ではない |
| Main | Heltec HT-HC01 V2 / MM6108 / 27 dBm級 | 機能・距離・電力の主開発。日本国内では適合確認前に通常の空中線送信を行わず、シールド環境・導通試験または適法な地域で評価する |
| Japan side line | FGH100M-J / MegaChips MRF61_A | 日本向け製品化を並走検討。認証、antenna、国内band、出力、Duty、供給条件を実機・一次資料で検証する |

最初にWM6180を使うのは、はんだ付けや自作carrierなしで、公式pin/BCF設定からsoftwareを検証できるためです。WM6180での成功はHC01 V2固有の電源・PA・RF性能を証明しません。HC01 V2を主系にするのは、PA/LNAを含むRF性能と入手性を評価するためで、日本で販売・使用する製品がHC01 V2のまま成立する、という意味ではありません。

## 最初の成功条件

2台のブレッドボードで次を再現できたら P0 完了です。

1. Node AでPTTを押す
2. 16 kHz mono音声をIMA-ADPCM化してUDP送信する
3. Node Bのスピーカーから100〜150 ms以内を目標に再生する
4. パケット損失、遅延、消費電力、RF airtimeを記録する

LCD、GPS、Opus、録音、メッシュはP0の完了条件に含めません。

## リポジトリ構成

```text
RoadWeave/
├── docs/                    設計判断、仕様、試験計画
│   └── decisions/          変更理由を残すADR
├── firmware/                ESP-IDFアプリケーション
│   ├── components/         rwp、audio_pipeline、opus
│   ├── experiments/        opus_bench、audio_bench、voice_udp
│   ├── sim/                host simulator（floor + voice over lossy channel）
│   └── boards/             module 別の profile / BCF
├── hardware/
│   └── kicad/              回路図・PCB・製造出力（今後追加）
└── tools/                   rwp_peer.py（Mac 側 echo / record / send）
```

## 設計文書

- [アーキテクチャ](docs/architecture.md)
- [BOM・100/1,000/10,000台コスト](docs/bom.md)
- [電力・バッテリー予算](docs/power-budget.md)
- [ESP32-S3 GPIO割り当て案](docs/gpio-allocation.md)
- [macOS / Windows / Linux CI開発環境](docs/development-environment.md)
- [KiCad回路図ブロック案](docs/kicad-schematic-plan.md)
- [PoCロードマップ](docs/poc-roadmap.md)
- [実行スケジュール](docs/schedule.md)
- [音声ネットワーク設計](docs/voice-networking.md)
- [GPS隊列・breadcrumb・PMTiles構想](docs/gps-and-maps.md)
- [XIAO + WM6180初回起動手順](docs/bringup/xiao-wm6180-first-boot.md)
- [P0-A software検証記録](docs/bringup/p0-a-software-validation.md)
- [Audio bench notes](docs/bringup/audio-bench-notes.md)
- [Opus benchmark 2026-09-05](docs/bringup/opus-bench-2026-09-05.md)
- [voice_udp 計測 2026-09-06](docs/bringup/voice-udp-2026-09-06.md)
- [消費電力の計測手順](docs/bringup/power-measurement-plan.md)
- [ADR-0001: 開発ラインと段階戦略](docs/decisions/0001-development-lines.md)
- [ADR-0002: WM6180をP0 reference platformにする](docs/decisions/0002-wm6180-reference-platform.md)
- [ADR-0003: HC01P V2 + HATをbreakoutにする（Proposed）](docs/decisions/0003-hc01p-hat-breakout-and-linux-bridge.md)

## 現時点の重要ゲート

- Morse MicroのESP-IDF向けHaLowコンポーネントはプレリリース。P0-AはESP-IDF `v5.4.4`とcomponent `2.11.2-esp32-2`を固定し、XIAO + WM6180公式profileから検証する。
- 購入したWM6180は902–928 MHz系。日本国内で通常の空中線送信をせず、RF fixtureと適法な試験条件を先に確定する。
- **HC01 V2用BCFとファームウェアの組み合わせを実機確認するまで、製品主系の採用は確定しない**。
- HC01 V2は3.3 V系とPA用5 V系を持つ。27 dBm・100% TXでは5 V側が最大約400 mA級になるため、平均値だけで電源を設計しない。
- 日本向けはMRF61_Aを別BOM・別RF試験として維持する。モジュールの技適だけで、アンテナ条件、最終製品のEMC・安全・表示など全要件が自動的に完了するわけではない。
- 地図PoCは地図データなしの隊列・Radar・breadcrumbから始める。PMTilesはmicroSDと描画負荷の実測後に判断する。

## 主要な一次資料

- [Heltec HT-HC01 V2 product page](https://heltec.org/project/ht-hc01-v2-wifi-halow-module/)
- [Heltec HT-HC01 V2 datasheet Rev.2.0](https://resource.heltec.cn/download/HT-HC01_V2/Datasheet/HT-HC01_V2.pdf)
- [Morse Micro HaLow ESP-IDF component](https://components.espressif.com/components/morsemicro/halow)
- [ESP32-S3-WROOM-1 datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf)
- [MegaChips MRF61_A product page](https://www.megachips.co.jp/product/modules/wifi-halow/rf-modules/)

## ステータス

2026-09-06 時点:

- XIAO ESP32S3 1 台目: Gate 1（USB/flash/PSRAM smoke）PASS。残り 2 台と Wio-WM6180 x3 は到着待ち。
- ESP-IDF v5.4.4 を恒久インストール（`~/esp/v5.4.4/esp-idf`）。porting assistant と Gate 4 用 example（softap / sta_connect / iperf）は XIAO profile でビルド済み。
- P0-B の部品を先行実装: RWP/0.1 + floor control（`components/rwp`）、IMA-ADPCM + jitter buffer（`components/audio_pipeline`）、host test と host simulator（`firmware/sim`）を CI で実行。
- Opus は XIAO 単体で実測済み（[結果](docs/bringup/opus-bench-2026-09-05.md)）。
- 音声パイプラインを内蔵 2.4 GHz Wi-Fi で Mac と往復する実験（`experiments/voice_udp`）を初回計測（[記録](docs/bringup/voice-udp-2026-09-06.md)、アンテナ未装着）。
- HC01P V2 の mini PCIe pin map と Heltec BCF を `firmware/boards/heltec_hc01p/` に配置。HAT 40 pin は実測待ち。

本文書の金額・電力・通信性能は、購入記録または実測値と明記したものを除き設計仮定である。
