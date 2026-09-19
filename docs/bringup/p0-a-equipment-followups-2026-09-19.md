# P0-A 電源試験・計測器の準備（2026-09-19）

ユーザー申告では電源制御設備がなく、手動50回の抜き差しも実施不可。電流・電圧の測定器も未保有で、別途購入が必要。
本日の実証からこの2項目を外し、将来課題として保存した。購入・発注は実施していない。

## 実電源断50回 — Issue #27

[Issue #27](https://github.com/rimtty/RoadWeave/issues/27)では、2組それぞれ電源OFF→ONを50回実施する。
必要なのは端末のVBUSを実際に断つswitch／relay／対応hub、または実施可能になった時点での手作業。
OSのdevice disable、再列挙、DTR/RTS resetは電源断の証拠にしない。

選定前に、ポート単位の給電遮断、USBデータ経路、復電時の再列挙、別経路から給電されない配線を確認する。
試験時はOFF保持時間、操作記録、個体、boot/reset reason、MM6108初期化結果を保存する。
当日のcontrolled software restartやサービス停止の成功は、この50回へ加算しない。

## 電流・rail電圧 — Issue #28

[Issue #28](https://github.com/rimtty/RoadWeave/issues/28)の購入前メモ。型番はまだ選定していない。

| 測定目的 | 機器・治具の候補種別 | 選定時に確認する仕様 |
|---|---|---|
| USB入力全体の平均電流・電力 | USB-Cインライン電力計／logger | USBデータを通すこと、測定レンジ・分解能・誤差、記録間隔、CSV等の保存、挿入時の電圧降下 |
| モジュールrailの平均と時間変化 | 電流計測器／shunt＋logger、必要な配線治具 | 測定rail・電流レンジ、shunt負担電圧、実際の変換時間と連続sampling、校正条件 |
| 短いTX burstや電圧droop | 十分な帯域の電流計測器、またはoscilloscope＋shunt／電流probe | 対象pulse幅に対する帯域・sampling・trigger、同時電圧観測、配線による負担 |

低速USB表示の最大値や1 kHz samplingという設定だけで、短いRF burstの真のpeakを測れたとは判断しない。
機器を選んだ後にデータシートの変換時間・平均化・帯域・誤差を確認し、測定できる時間尺度を明記する。
既存の[電力計測手順](power-measurement-plan.md)の機器例・精度表は未検証の計画例であり、購入仕様や実測保証にはしない。

最初にUSB入力全体と3.3 Vモジュール単体のどちらを測るか決める。
接続済みidleはアプリ送信なし・HaLow省電力無効と定義し、deep sleepと区別する。
echo試験はTX/RX混合であり、分離できない場合は混合負荷として記録する。
機器型番・測定点・sampling/帯域・平均/取得可能なpeak・電圧・FW/binary・無線設定を保存する。
BUSY/WAKE未配線に対するsleep vetoの電力影響は未測定のまま残す。
