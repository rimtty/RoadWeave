# 製品定義 v2（最終形モックの吟味）

更新日: 2026-09-06  
根拠: 2025-05-24 開発計画書と「車と車をつなぐ IP-PTT ラジオ」モック、2026-09-05〜06 の実測
（[Opus](bringup/opus-bench-2026-09-05.md)、[voice_udp](bringup/voice-udp-2026-09-06.md)）、[920 MHz 制度](regulatory-920mhz-japan.md)。

## 1. モックから確定させる仕様と、変える仕様

| 項目 | モック（2025-05） | v2 の決定 | 理由 |
|---|---|---|---|
| 形状 | ハンディ 56×144×28 mm、約 150 g | **60×140×28 mm、約 170 g** | 2.8 インチ表示を入れるため幅 +4 mm |
| 表示 | 1.47 インチ IPS 172×320、非タッチ | **2.8 インチ IPS 240×320、静電容量タッチ** | 隊列・レーダー・ルート地図を走行中に一目で読むには 1.47 では狭い。設定・グループ操作をタッチにすると物理ボタンを減らせる。原価差は 100 台で約 +1,000 円 |
| 操作 | PTT、VOL±、機能ボタン、LED | **PTT（大）、VOL±、電源/機能、状態 LED、タッチは停車時用** | 走行中はタッチに依存しない（architecture.md §7） |
| 無線 | Wi-Fi HaLow 27 dBm 級 / SMA | **MRF61_A（13 dBm、技適済み）/ SMA（認証登録アンテナ）** | 国内で免許不要で売れる唯一の構成。920.5〜923.5 MHz を factory lock |
| グループ | 3〜4 台 | **通常 6〜8 台、上限 16 台** | ユーザー要件「最低 6 人で 1 グループ」 |
| 同時発話 | 1（PTT） | **通常 1、最大 2（voice room モード）** | MCU は 6 本デコードできるが、1 MHz MCS0 の無線帯域は 2 本が限度 |
| 音声コーデック | Opus 16 kbps mono | **Opus 16 kHz、12〜16 kbps、encoder complexity 0〜1、decoder は本数制限なし** | c3 で 6 本デコードすると core 0 の 60% を codec が占める。c0〜1 なら 37% |
| GPS | NEO-M8N、2.5 m CEP | 同等品（M8N 互換 / ATGM336H）、1 Hz beacon | 変更なし |
| 音声 I/O | Audio codec ES8388 + 内蔵マイク/スピーカー | **I2S MEMS マイク + MAX98357A（5 V 給電、4 Ω 2 W）** | codec IC を省き、PoC と同じ部品で製品化。安価で実績あり |
| バッテリー | 2,500 mAh | **3,000 mAh 1S LiPo** | 2.8 インチのバックライト分（+0.1 W）を吸収して 12 時間を確保 |
| 充電/データ | USB-C 5 V | USB-C 5 V、車載給電時はバッテリー・バイパス | 変更なし |
| 防塵防水 | IP54 | IP54（Rev.A）、IP67 は将来 | 変更なし |
| 動作温度 | -10〜50 ℃ | 同左（LiPo の充電は 0〜45 ℃） | 変更なし |
| 録音 | SD 録音（基本） | microSD パッドは Rev.A に残し、機能は P5 | 変更なし |

## 2. 6 人グループの成立条件（実測に基づく）

- **MCU**: ESP32-S3 で 1 encode + 6 decode は complexity 0 で core 0 の 36.6%、c3 で 60.1%（実測）。UI は core 1。
  → 6 人同時受信でも MCU は足りる。encoder は c0〜1 に固定し、音質はビットレート（12→16 kbps）で稼ぐ。
- **無線帯域**: 1 stream ≈ 38 kbps wire（Opus 12 kbps + RWP 36 B + UDP/IP 28 B、20 ms）。
  1 MHz MCS0（PHY 300 kbps）では同時 2 stream が実用上限。2 MHz MCS2 以上なら 4〜6 stream も入るが到達距離が落ちる。
  → **同時発話は 2 まで**（floor control で 3 人目は BUSY）。位置ビーコンは 6 台 × 28 B / s で無視できる。
- **メモリ**: 1 enc + 6 dec で内部 RAM 132 KB、jitter buffer 6 本は PSRAM。LVGL 描画バッファ 38 KB は内部 DMA RAM。
- **制度**: 920.5〜923.5 MHz は総送信時間の制限なし（4 s 以内・休止 50 ms・CS 5 ms）。連続会話は MAC 層のバースト分割で適合。

## 3. 表示と UI

- 2.8 インチ IPS 240×320 縦、静電容量タッチ（FT6336 系、I2C）、輝度 300 cd/m² 以上、バックライト PWM 25 kHz。
- LVGL 9（MIT）。画面は GROUP / RADAR / CONVOY / ROUTE（P3〜）/ SETTINGS（停車時のみ）。
- `firmware/experiments/ui_lvgl` の 3 画面が土台。1.47 インチの UI 資産は捨てる（まだ作っていない）。
- 日光下: 反射防止フィルム付きレンズ、輝度自動（照度センサは Rev.B 検討）。

## 4. モックのまま残す価値

モックの「大きな PTT」「縦画面」「SMA アンテナ」「USB-C」「IP54」は v2 でも同じ。
1.47 インチ非タッチの「Lite」SKU は、v2 のファームをそのまま載せて画面だけ変えれば作れる（原価 -1,000 円）。最初の量産では SKU を増やさない。

## 5. 設計へ反映する箇所

- [BOM v2](bom.md): 表示・タッチ・バックライト・MRF61_A を反映、3 数量帯で再計算
- [事業性](business-case.md): 価格・粗利・NRE・損益分岐
- [電力予算](power-budget.md) §9: 2.8 インチと 6 stream の影響
- [GPIO 割り当て](gpio-allocation.md) §2: タッチ I2C / INT、バックライト
- [PoC roadmap](poc-roadmap.md): P1 を 6 node に
- GitHub: Issue #19〜#21（表示調達と LVGL、6 node 用追加ハード、事業性レビュー）
