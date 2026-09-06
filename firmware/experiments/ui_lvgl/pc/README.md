# Windows LVGL simulator

RoadWeaveの実機用 `ui_screens.c` をWindowsのウィンドウで描画する。
LVGL **9.5.0**、SDL **2.32.10**、MSVC、CMake/Ninjaを使用する。
画面は240×320 / RGB565、ソフトウェア描画。画面コード・フォント・位置計算・基本の模擬データ生成をESP32-S3と共用する。

## 起動

RepoルートのPowerShellで実行する。ESP-IDF環境の起動は不要。

```powershell
.\firmware\scripts\ui-sim.ps1
```

初回はLVGLとSDLの指定コミットを `.tools/` に取得してビルドする。
2回目以降は差分ビルドして起動する。Visual Studioの「C++によるデスクトップ開発」が必要。
同じcheckoutのシミュレーターが起動中なら、Run / Build / Test / Captureはそのプロセスを終了してからビルドする（模擬状態はリセット）。
CMake/NinjaはPATH上のもの（Visual Studio同梱を含む）を使用し、見つからなければこのPCの `D:\Espressif\tools` を探す。
依存ライブラリのバージョン違いやローカル編集は検出して停止する。USB・無線・接続先情報は使用しない。

```powershell
# レーダーを3倍表示、模擬時刻を停止して起動
.\firmware\scripts\ui-sim.ps1 -Scenario radar -Zoom 3 -Paused

# コンパイルだけ
.\firmware\scripts\ui-sim.ps1 -Action Build
```

実行ファイルは `pc/build/roadweave_ui_sim.exe`。ビルド後はこのexe単独でも起動できる。
保存先を一定にするには上記スクリプトから起動する。

## 操作

| 操作 | 内容 |
|---|---|
| マウス | タッチ相当。PTTを押している間は模擬TX。ボタン外への移動・解放で解除 |
| リスト内の縦ドラッグ | 8台表示などで隠れた行を表示。下部のボタンは固定 |
| `1` / `2` / `3` | GROUP / RADAR / CONVOY |
| `F1` / `F2` / `F3`、または `Z` | 等倍 / 2倍 / 3倍。`Z`は順番に切替。論理解像度は240×320のまま |
| `Space` / `.` | 模擬データの時刻を停止・再開 / 停止して100ms進める |
| `R` | 模擬時刻を0に戻し、模擬PTT・ミュートをリセット |
| `S` | 240×320のPNGを保存。通常は `pc/build/captures/` |
| `G` / `0` / `8` | 通常3台 / 他車0台 / 他車8台 |
| `D` / `L` | 古い位置情報 / 通信断・電池残量5% |
| `X` / `T` / `B` | RX / TX / BUSY |
| `Esc` / ウィンドウを閉じる | 終了 |

MUTEとPTTは模擬モデルだけを変更する。`T`の固定TXシナリオはPTT解放後もTXになるため、通常操作の検証には`G`を使う。
一時停止は模擬データに対するもので、マウス入力・ボタンの描画処理は動き続ける。
文字のショートカットは通常のキーイベントと文字入力イベントの両方に対応し、両方が届く場合の二重実行を抑止する。リモート操作などでファンクションキーが届かない場合も、`Z`で倍率を変更できる。

## 繰り返しテスト

```powershell
.\firmware\scripts\ui-sim.ps1 -Action Test
```

16テストを実行する。

- **画像13種類**：group / radar / convoy / empty / max_group / max_convoy / stale / radar_east / radar_far / rx / tx / busy / link_down。
- **操作**：3画面の巡回、PTTの押下・長押し・解放・ドラッグアウト・画面変更、MUTE切替、最大人数のリストスクロール、人数の範囲外入力。
- **SDLバックエンド**：SDLのマウスイベントを注入し、等倍・2倍・3倍それぞれの座標変換、倍率変更前後の描画バッファ一致、キーボード・文字入力と二重実行の抑止、PNG保存を検証。自動実行時はdummy video driverを使用。
- **比較処理自体の検証**：異なる画像と基準画像の欠落が失敗になり、基準画像を自動更新しないことを検証。

画像テストは模擬時刻0秒、LVGLの仮想時間300msで固定。
LVGLの `lv_test_*` で入力・時間を模擬し、基準画像とのピクセル比較は許容差0。
Windowsのウィンドウ枠・拡大率・DPIは比較に含まれない。
基準画像を変えるときはLVGLのバージョン、フォント、色深度の変更も合わせて確認する。

結果は `pc/build/results/` に保存する。
各シナリオの `actual.png` が今回の描画、`reference.png` が比較用コピー。
比較失敗時は `reference_err.png` と、変化したピクセルをピンクで示す `diff.png` を保存する。
`junit.xml` はCTestの集計。`compare_contract/` 内の差分画像は意図的に失敗させた比較処理の検証結果。

## 画面修正後の基準画像更新

1. 画面コードを修正する。
2. 候補画像を生成し、文字・距離・重なり・表示状態を目視確認する。
3. 意図した変更なら、確認済みの候補を基準画像にコピーする。
4. テストを再実行し、コードと基準画像を一緒にレビューする。

```powershell
.\firmware\scripts\ui-sim.ps1 -Action Capture -All
# pc/build/candidates/*.png を目視確認
.\firmware\scripts\ui-sim.ps1 -Action AcceptBaselines -All
.\firmware\scripts\ui-sim.ps1 -Action Test
```

1画面だけ更新するときは `-All` の代わりに `-Scenario radar` などを指定する。
**Run / Test / Captureは基準画像を更新しない。** 基準画像は `pc/baselines/` でGit管理する。

## 構成

| ファイル | 役割 |
|---|---|
| `../main/ui_screens.c` | 実機・PC共通の画面生成、描画、タッチイベント |
| `../main/ui_simulation.c` | 実機・PC共通の時刻から決まる模擬データ |
| `../main/ui_main.c` | ESP-IDF、液晶、タッチ、SPI、FreeRTOSの初期化 |
| `main.c` | Windowsウィンドウ、キー操作、実行モード |
| `scenarios.c` | 固定シナリオと操作・表示内容の検証 |
| `capture.c` | RGB565描画バッファからPNG・差分画像を生成 |
| `lv_conf.h` | PC用LVGL設定。画像比較用の大きなメモリ領域とテスト機能を有効化 |

新しいシナリオは `scenarios.c` に追加し、CMakeLists.txtと `ui-sim.ps1` の一覧にも追加する。
画面コードのコピーを作らず、両ビルドから同じファイルをコンパイルする。

## 検証記録（2026-09-06）

- WindowsのMSVCビルド成功。実ウィンドウでの起動・終了を確認。
- 実ウィンドウのマウスによるNEXT切替と、Windows操作ツールからの数字キーによる画面切替、Spaceの停止・再開、XのRX切替、Zの倍率変更、SのPNG保存を確認。保存PNGの描画も目視確認。入力診断にはexeの `--trace-input` を使える。
- Windows操作ツールから文字入力だけが届くケースに対応。倍率変更時に固定ボタンなどが消える問題も、画面全体の再描画で修正し、回帰テストを追加。
- 13種類のPNGを目視確認して初期基準画像を登録。CTest 16/16成功。
- 日本語MSVCのincludeメッセージを正しく認識するようCMakeで補正し、ヘッダー変更時の差分ビルドに対応。
- ESP-IDF v5.4.4 / ESP32-S3向けもビルド成功。USB書き込みは実施していない。
- シミュレーターで見つかった既存UIの問題を修正：PTTのドラッグアウト、人数超過時の配列アクセス、リストと下部ボタンの重なり、ヘッダー列、レーダーの固定N表示・ラベル重複、古い距離の確定表示。

GitHub Actionsの `LVGL Windows simulator` ワークフローも追加。
ローカルで検証済みだが、この変更のリモートCI実行はまだ行っていない。
ESP32上のFPS・RAM使用量・SPI転送時間・音声との同時動作はこのPC実行では測定できず、実機ベンチで確認する。

公式資料：[LVGL PC port](https://github.com/lvgl/lv_port_pc_vscode)、[LVGL 9.5 test APIs](https://github.com/lvgl/lvgl/tree/v9.5.0/src/debugging/test)。
