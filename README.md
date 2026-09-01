# RoadWeave

RoadWeave は、インターネットや携帯回線がない場所でも、ツーリング中の車両どうしでグループ音声と位置情報を共有する小型端末の研究開発プロジェクトです。

製品の核は **ESP32-S3 + Wi-Fi HaLow + IP-PTT** です。最初は2台のブレッドボードで低遅延の半二重音声を成立させ、そこから「オフラインのDiscord風ボイスルーム」、指定相手へのPTT、個別ミュート・音量、音声記録、GPS隊列表示へ段階的に育てます。

## 開発ライン

| ライン | 無線モジュール | 位置づけ |
|---|---|---|
| Main | Heltec HT-HC01 V2 / MM6108 / 27 dBm級 | 機能・距離・電力の主開発。日本国内では適合確認前に通常の空中線送信を行わず、シールド環境・導通試験または適法な地域で評価する |
| Japan side line | MegaChips MRF61_A / MM6108 / 技適取得済み | 日本向け製品化を並走検討。13 dBm、国内バンド・Duty制約を実機で検証する |

HC01 V2を主系にするのは、PA/LNAを含むRF性能と入手性を早く評価するためです。日本で販売・使用する製品がHC01 V2のまま成立する、という意味ではありません。

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
├── hardware/
│   └── kicad/              回路図・PCB・製造出力（今後追加）
└── tools/                   設計・試験補助ツール（今後追加）
```

## 設計文書

- [アーキテクチャ](docs/architecture.md)
- [BOM・100/1,000/10,000台コスト](docs/bom.md)
- [電力・バッテリー予算](docs/power-budget.md)
- [ESP32-S3 GPIO割り当て案](docs/gpio-allocation.md)
- [KiCad回路図ブロック案](docs/kicad-schematic-plan.md)
- [PoCロードマップ](docs/poc-roadmap.md)
- [音声ネットワーク設計](docs/voice-networking.md)
- [GPS隊列・breadcrumb・PMTiles構想](docs/gps-and-maps.md)
- [ADR-0001: 開発ラインと段階戦略](docs/decisions/0001-development-lines.md)

## 現時点の重要ゲート

- Morse MicroのESP-IDF向けHaLowコンポーネントはプレリリース。ESP32-S3/MM6108は対象だが、**HC01 V2用BCFとファームウェアの組み合わせを実機確認するまで採用確定しない**。
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

現在は設計・部品調達前のP0準備段階です。本文書の金額・電力・通信性能は、明記のない限り2026-09-02時点の設計仮定であり、発注見積や実機測定値ではありません。
