# P0-A HaLow実証結果 — 2026-09-19

**有限通信・DHCP・復帰・帯域幅比較の当日実証を完了した。** AP software restartで発見したGPIO割り込みwatchdogの再起動loopを修正し、修正版で復帰・1/2 MHz各3回・追加負荷・3台同時通信を確認した。P0-A全体には長時間運転と設備・現物確認の残件がある。

計画・管理・独立レビューはAstra、実装はSol、実機操作と統合は主担当。8時間運転は対象外。内蔵Wi-Fiを使わず、ESP32-S3→SPI→MM6108→HaLowで全試験を実施した。

## 条件と根拠

| 項目 | 条件 |
|---|---|
| 実機 | COM4 AP、COM5 STA ID1、COM6 STA ID2。ESP32-S3＋Wio-WM6180、アンテナ装着はユーザー申告 |
| GPIO設定 | SCK7・MOSI9・MISO8・CS4・IRQ3・RESET_N1、状態LED21。BUSY/WAKE未配線 |
| SDK | ESP-IDF v5.4.4、morsemicro/halow 2.11.2-esp32-2 |
| RF | 使用条件確認済みの既存申告を継続。設定上限1 dBm、実測出力ではない |
| 基本負荷 | UDP exact echo、payload256 B、間隔250 ms。予定8,192 bit/sで最大速度試験ではない |
| baseline基準 | 10秒warmup後120秒以上、200送信以上、受信率99%以上、予期しないresetなし、正常終了 |
| 比較 | 1 MHz/ch3/903.5 MHzと2 MHz/ch6/905 MHzを修正版の同一sourceで各3回 |
| 記録 | 生ログはprivate保存。公開用metricsのsource・binary SHA256・counter epochから追跡可能 |

中心周波数も変わるため、比較差を帯域幅だけの効果と断定しない。RTTはUDP往復であり、音声遅延ではない。goodputはこのstop-and-wait負荷での値。

## 当日判定

| 計画 | 判定 | 結果／残件 |
|---|---|---|
| T01 基本回帰 | PASS | core版でCOM5/COM6とも20/20。両端無線停止・PASS・DONE |
| T02 有限継続 | PASS | 修正版で両幅各3回、warmup後基準を満たした。修正前COM6の1応答欠落も保持 |
| T03 AP再起動3回 | PASS（修正後） | aa58241で3/3。AP_READY後2.734/4.703/2.828秒でecho復帰。元FAILも保存 |
| T04 STA再起動3回 | PASS | 3/3、BOOT後3.891/5.859/4.969秒でecho。全4epoch合計1014/1014 |
| T05 APサービス10秒停止×3 | PASS | 3/3、再開後22.125/8.219/19.437秒でecho復帰。device outageは別記 |
| T06 DHCP | PASS（実施範囲） | 両STA順次と3台同時、AP/STA再起動・APサービス再開後のfresh lease・UDP bindを確認 |
| T07 1/2 MHz各3回 | PASS（取得可能指標） | 修正版で各3/3 PASS。TX再送総数は公開SDK APIから未取得 |
| T08 実電源断50回 | 当日延期 | ユーザー指示。[Issue #27](https://github.com/rimtty/RoadWeave/issues/27) |
| T09 software reset反復 | PASS（補助試験） | AP3回＋STA1 4回＋STA2 1回、計8回のcontrolled restart成功。実電源断とは別 |
| T10 起動警告 | PASS（分類・短時間観測） | 起動順序・SDK source・後続通信を照合。反復でも各bootの件数は一定。原因解消とは扱わない |
| T11 電流・電圧 | 当日延期 | 測定器未保有。[Issue #28](https://github.com/rimtty/RoadWeave/issues/28) |
| T12 受入・LED目視 | 一部未確認 | 3個体通信確認済み。ラベル／写真／アンテナ詳細／LED現物確認待ち |
| X01 3台同時 | PASS | 修正版baselineと両STA交互再起動時の相手の継続を確認 |
| X02 追加負荷 | PASS（有限負荷） | 1024 B/100 ms、両幅でwarmup後1369/1369、約82 kbit/s |

## 主な実測

以下は修正前source `f9559a3`、1 MHz、DHCP。詳細は[metrics](logs/2026-09-19-validation/metrics.json)に分離する。

| case／STA | 受信/送信 | warmup後 | exact RTT p50/p95/max (ms) | goodput (bit/s) | scan SNR (dB) |
|---|---:|---:|---|---:|---:|
| dhcp-1mhz-run1／COM5 | 577/577 | 547/547、136.953秒 | 16.604 / 28.346 / 42.614 | 8,039 | 44 |
| dhcp-com6／COM6 | 570/571 | 542/543、136.797秒 | 22.595 / 50.616 / 155.411 | 7,946 | 45 |
| dhcp-three-node-original／COM5 | 576/576 | 548/548、137.016秒 | 16.920 / 30.708 / 54.414 | 8,019 | 39 |
| dhcp-three-node-original／COM6 | 575/575 | 547/547、136.891秒 | 26.642 / 52.035 / 77.980 | 8,010 | 48 |

COM6単独のwarmup後受信率99.8158%は暫定99%基準を満たすが、loss1をゼロ扱いにしない。`reconnects=1`はprobe timeout後のアプリ応答回復で、途中のL2切断を示すSTA_STATE変化はない。

3台同時ではSTA1 `.2`、STA2 `.3` の異なるDHCPアドレスを取得し、AP echo_id1=576・echo_id2=575と各STAログが一致した。全端末が正常に終了した。

SNRはscan時の同一probeのRSSI−noise。−127〜−1 dBmのsanity条件を満たした時だけ算出し、連続通信中のSNRや校正済み測定とは扱わない。TX再送総数をRC送信−成功から推定しない。短時間のheap推移で長時間安定性を保証しない。

### 同一修正版による1/2 MHz比較

COM4/COM5、source aa58241、payload256 B/250 msを固定。各runの10秒warmup後を比較し、初回接続待ちを含む全期間の値はmetricsへ別記する。

| 幅／run | 全期間応答 | warmup後応答・時間 | warmup後exact RTT p50/p95/max (ms) | 同goodput (bit/s) | scan RSSI/noise/SNR |
|---|---|---|---|---:|---|
| 1 MHz／1 | 577/577 | 547/547・137.015秒 | 16.274 / 27.599 / 42.366 | 8,176.16 | −39/−72 dBm /33 dB |
| 1 MHz／2 | 577/577 | 547/547・136.922秒 | 16.370 / 28.173 / 41.557 | 8,181.71 | −40/−94 dBm /54 dB |
| 1 MHz／3 | 569/569 | 547/547・136.938秒 | 16.205 / 27.027 / 42.835 | 8,180.75 | −40/−86 dBm /46 dB |
| 2 MHz／1 | 577/577 | 547/547・136.984秒 | 13.962 / 24.160 / 45.887 | 8,178.01 | −36/−80 dBm /44 dB |
| 2 MHz／2 | 577/577 | 547/547・136.985秒 | 14.057 / 25.088 / 39.211 | 8,177.95 | −38/−80 dBm /42 dB |
| 2 MHz／3 | 578/578 | 548/548・137.079秒 | 14.027 / 23.057 / 45.776 | 8,187.28 | −36/−80 dBm /44 dB |

1 MHz run3の全期間goodput7,927 bit/sはrun1/2の8,039 bit/sより低いが、初期接続・IP-ready待ちによるskipped19対11を含む。warmup後は各547/547で約8.18 kbit/sと同程度。周囲の電波条件を制御しておらずscan noiseも変動するため、帯域差やSNRと速度の因果をこの少数runだけで判断しない。

6回ともwarmup後のloss0、予定負荷に近い約8.18 kbit/s。今回の2 MHzのRTT中央値は約14 ms、1 MHzは約16 msだったが、中心周波数・時間帯・雑音も異なる。幅を広げたときの最大capacityを測った結果ではない。

RC開始値はNA。最初の有効SAMPLE→最後のSUMMARYの単調な区間に限ったraw差分は1 MHz run1 603送信/550成功、run2 625/550、run3 612/550。UDP層の547/547とは別の計数であり、差をTX再送回数としない。AP側RCは未取得。

### 追加負荷試験

payload1024 B/100 msは予定片道81,920 bit/sで、echo返信も同程度のpayloadを運ぶ。音声codec・20 ms frame・PTTの実証ではなく、最大capacity測定でもない。

| 幅 | 全期間応答 | warmup後応答・時間 | warmup後goodput (bit/s) | 同skipped | 同exact RTT p50/p95/max (ms) |
|---|---|---|---:|---:|---|
| 1 MHz | 1443/1443 | 1369/1369・136.828秒 | 81,963.11 | 0 | 28.519 / 41.305 / 60.660 |
| 2 MHz | 1444/1444 | 1369/1369・136.750秒 | 82,009.86 | 0 | 22.381 / 32.032 / 58.688 |

全期間のFW goodputは1 MHz 80,426 bit/s、2 MHz 80,482 bit/s。skipped27/26はwarmupまでに発生した。表はhostのSAMPLE→SUMMARY時間窓で計算し、warmup後は予定負荷に近い約82 kbit/sで両幅とも欠落0・skipped0。有限窓のpacket境界とログ受信時刻によりnominalをわずかに超える値になるため、設定速度以上の持続能力を証明したとは扱わない。

### 修正版による3台同時通信

`fixed-three-node-baseline` は共有warmup後136.844秒でSTA1 548/548、STA2 547/547となり、同時観測の基準を満たした。全期間はSTA1 576/576、STA2 570/571。STA2の初期1欠落とアプリ応答回復181 msを保持し、L2切断とは扱わない。APの返信送信数576/571とSTAの受信数576/570には、この1応答分の差がある。

DHCPはSTA1 `.2`、STA2 `.3`、各bootの警告は2件ずつ、全3台正常終了。warmup後exact RTT p50/p95/maxはSTA1 16.436/28.591/47.220 ms、STA2 21.786/37.746/66.738 msだった。

続く230秒の `fixed-three-node-resets` でSTA1を30秒、STA2を110秒にcontrolled restartした。双方でACK→無線停止→BOOT reason3→fresh lease・UDP bind→echoを確認し、相手端末は次表のとおり継続した。

| 再起動端末 | BOOT→echo | 復帰後連続echo | 相手の再起動中echo数 | 相手の最大echo間隔 |
|---|---:|---:|---:|---:|
| STA1 | 3.891秒 | 295 | STA2: 23 | 0.547秒 |
| STA2 | 4.875秒 | 452 | STA1: 27 | 0.547秒 |

全epochを合算するとSTA1は95＋775＝870/870、STA2は414＋452＝866/866で、APのpeer別返信数と一致した。相手の停止・再接続・timeoutは障害窓内にないが、全runの最大RTTはSTA1 331.059 ms、STA2 309.606 msであり、音声遅延基準の達成は意味しない。最後に全3台のSTOP ACK・無線停止・PASS・DONEを確認した。

## 見つかった不具合と修正

修正前 `dhcp-ap-reset3` は初回RESTART ACK後にAPが割り込みwatchdog loopへ入り、復帰しなかった。保存rawにpanic70件、BOOT reason3×1・reason5×69を確認。両端の最終SUMMARY／正常終了はなく、FAILとして保存した。ホストもtimeoutのない後続writeで待機し、手動中断したため最終events/reportは欠落している。

一致するELFによるbacktraceはGPIO ISR service設置中を示した。SDKはSPI IRQを無効化する前にISR serviceを設置する順序で、CPU reset後のIRQ設定残存と整合する。修正 `aa58241` はRF/preflight共通起動でIRQ3を無効化・pending statusをclearし、RESTARTはradio停止成功後に実行する。新たな検証範囲はcontrolled software restartであり、突然の電源断や任意のpanicとは区別する。

host修正 `2a49444` はwrite timeout2秒、短いwriteの検査、events逐次保存を追加した。独立offline31テスト成功。

修正後 `fixed-ap-reset3` は3/3成功。各回でACK→radio停止→RESTART_READY→BOOT reason3→DHCP server開始→新lease・UDP再bind→echoを確認した。AP_READY後の復帰は2.734/4.703/2.828秒、以後291/285/333連続応答。STA全体は1008/1009で障害区間の1欠落を保持し、全端末正常終了。端末報告outage6.780/7.795/6.783秒は別の開始点を持つため、上記復帰時間と区別する。

`fixed-sta-reset3` も3/3成功。各BOOT後3.891/5.859/4.969秒でfresh DHCP lease・UDP bind・echoを確認し、295/287/332連続応答。STA各epochは98/98＋296/296＋288/288＋332/332で、全体1014/1014・lost0。最終SUMMARYの332/332を全runの値にはしない。AP echo1014と一致し、両端正常終了。

`fixed-ap-off3` は3/3成功。各回でDHCPS停止→AP service off→on・DHCPS開始→fresh lease・UDP bindを確認した。AP_READY後22.125/8.219/19.437秒でecho復帰、以後184/236/239連続応答。device outageは33.777/20.798/30.039秒。759/759応答だが停止中などのskipped349があり、無停止通信の意味ではない。指定holdは10秒、実off→onは遷移処理を含み11.593/12.594/10.594秒だった。

別件でcore/static baselineの旧harnessが準備中STA autobootを本番に混入しFAILとした。rawと旧FAILを保持し、各端末の最初の明示resetを境界とする修正版で別reportを生成してPASSを確認した。最後のbootだけを選んで異常resetを隠す方式は用いていない。

## 起動警告と未完了事項

正常baselineでは本番bootごとにaddress-base警告2回、unknown TLV6警告2回があり、SDK placeholderと選択channelでのradio再bootに各1回対応する。後続chip reset・FW起動・通信成功を確認したが、警告原因の解消や全TLVの無害性は証明していない。BUSY/WAKE未配線へのsleep vetoは回避策で、物理信号検査や低消費電力達成ではない。

修正版AP/STA再起動の各bootでも同じ件数で、APサービス停止・再開3回では警告の追加増加はなかった。SDKのaddress-base書込み失敗は上位へ戻るが、reset処理は先行clock-control書込みの戻り値を検査せず、その後のchip-ID読出しで成否判定する。unknown TLVは未対応tagをlength分進めて解析を続ける。観測した回復との整合を確認したもので、未記録のレジスタやTLV6の意味を断定したわけではない。

8時間運転、実電源断50回、電流・電圧、現物受入の未確認分を残すため、P0-A全体完了とはしない。

## 保存と最終機器状態

| 成果 | PR | merge commit |
|---|---|---|
| 前段実測・残課題 | [#25](https://github.com/rimtty/RoadWeave/pull/25) | `abe1b35` |
| 事前計画 | [#26](https://github.com/rimtty/RoadWeave/pull/26) | `cc8ec4a` |
| 有限継続FW | [#29](https://github.com/rimtty/RoadWeave/pull/29) | `141a373` |
| 2台実行・集計tools | [#30](https://github.com/rimtty/RoadWeave/pull/30) | `4ae20277873fd4299dbfda595a20a4550b6162e6` |
| 明示initial resetの試験境界 | [#32](https://github.com/rimtty/RoadWeave/pull/32) | `614af7fe2c409aff79435b82dc2d09334b1edede` |
| 3台・判定・host復旧 | [#31](https://github.com/rimtty/RoadWeave/pull/31) | `824bd7d24fbca4a7132d6923dd6f6b3ac5802ec1` |
| DHCP | [#33](https://github.com/rimtty/RoadWeave/pull/33) | `43c934a79bdfd56e5a9ffe39d18c947798267fe6` |
| scan noise/SNR | [#34](https://github.com/rimtty/RoadWeave/pull/34) | `f142695c905e94a536011b9dbaa06b4849c72ff2` |
| warm restart修正 | [#35](https://github.com/rimtty/RoadWeave/pull/35) | `ae3919f1606ad8afc1e658969303b50ac90c2005` |

固定した実機sourceは `aa58241f266168e28041148cff8da38109ce2b69`。同sourceの12設定をビルドし、保存binary SHA256を独立照合した。
実機sourceは[firmware manifest](logs/2026-09-19-validation/firmware-manifest.json)と書込み時hashで追跡し、試験中のhost checkoutと混同しない。
[公開ログmanifest](logs/2026-09-19-validation/manifest.json)にallowlist出力とprivate原ログのhashを記録した。SSID/PSK、private sdkconfig、生ログは公開しない。
最終3台試験は以下の1 MHz・標準負荷・DHCP設定を使用し、全3台の無線停止を確認した。[最終機器記録](logs/2026-09-19-validation/final-devices.json)に書込みログhash・個体・binary・終了markerを対応付けた。

| ポート／ESP32 MAC | 役割 | aa58241 binary | SHA256先頭 | 終了状態 |
|---|---|---|---|---|
| COM4 / 44:B1:76:B0:57:20 | AP | dhcp-ap.bin | ab8ab6f0d7d94fe2 | 無線OFF・DONE |
| COM5 / 44:B1:76:B0:57:1C | STA ID1 | dhcp-sta.bin | 624bd0e5bffbc84e | 無線OFF・DONE |
| COM6 / 44:B1:76:AE:C4:90 | STA ID2 | dhcp-sta2.bin | b4a82989fdd9f0a8 | 無線OFF・DONE |

このFWはRF有効設定であり、再起動・再給電すると最大600秒のHaLow試験を開始する。STOPによる現在の無線停止と、フラッシュ上のRF無効設定を区別する。
