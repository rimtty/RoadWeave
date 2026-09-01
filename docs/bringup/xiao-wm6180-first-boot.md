# XIAO ESP32S3 + Wio-WM6180 初回起動手順

更新日: 2026-09-02  
対象: Seeed Studio XIAO ESP32S3（header実装済み）+ Wio-WM6180 x 3

## 目的

いきなりHaLow通信を始めず、故障箇所を1段ずつ切り分ける。各nodeを同じ順番で検査し、結果を[`p0-a-test-log.md`](p0-a-test-log.md)へ残す。

## 重要なRF注意

- XIAO側U.FLは2.4 GHz Wi-Fi/BLE用、WM6180側U.FL/IPEXは902–928 MHz HaLow用で、別のRF端子である。
- P0-AではXIAO側2.4 GHz antennaを使わない。HaLow antenna/terminationをXIAO側へ付けない。
- WM6180側RF端子を開放した状態でHaLow送信を開始しない。
- 2台のRF端子を同軸だけで直結しない。conducted testには50 ohm系の適切なattenuator、必要に応じてDC block、power rating確認が必要。
- 購入したFGH100M-H/WM6180は902–928 MHz系である。日本国内の通常空間へ電波を出す試験は行わず、適法なシールド/導通試験または専門設備を使う。
- `HALOW_COUNTRY_CODE=JP`を設定しても、北米向けmoduleが日本適合品に変わるわけではない。

## Gate 0: 開封・外観

電源を入れず、3組すべてで確認する。

- [ ] XIAOとWM6180の基板、shield、U.FLに破損・浮きがない
- [ ] header pinが曲がっていない、隣接pin間にはんだbridgeがない
- [ ] XIAOの5V/GND/3V3表示とWM6180 socketの表示を写真で記録
- [ ] nodeをRW-N01、RW-N02、RW-N03として識別できるようにする
- [ ] 902–928 MHz側のRF fixture/terminationを準備するまでHaLow実行を保留

## Gate 1: XIAO単体

WM6180を挿さず、XIAOだけをUSB-C data cableでMacへ接続する。

1. ESP-IDF `v5.4.4`をexportする。
2. `firmware/`でXIAO単体smoke firmwareをbuildする。
3. macOSの`/dev/cu.usbmodem*`へflashし、monitorを開く。
4. `P0A_XIAO_SMOKE=PASS`と`HALOW_TX=DISABLED`を確認する。
5. Flash 8 MiB、PSRAM 8 MiB、USB再接続後のboot logを保存する。
6. 3台すべて同じ結果になるまで次へ進まない。

```bash
cd firmware
./scripts/check-toolchain.sh
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/cu.usbmodemXXXX flash monitor
```

portが現れない場合はUSB data cableを確認し、XIAOのBOOTを押したままRESETを1回押してdownload modeへ入れる。GPIO19/20はnative USB用なので他用途へ割り当てない。

## Gate 2: 無通電でstack確認

1. USB-Cを抜く。
2. XIAOとWM6180の5V、GND、3V3のlabelが同じpin列に来る向きで、offsetなしに挿す。
3. 斜め挿し、1列ずれ、上下逆がないことを別角度の写真で確認する。
4. multimeterのcontinuityでGNDを確認し、5VとGND、3V3とGNDが短絡していないことを確認する。
5. この段階ではXIAO単体smoke firmwareのままにする。WM6180のSPI/RESETを操作しない。

## Gate 3: WM6180 bus bring-up

RF safety gateを満たしてから、公式`porting_assistant`を生成する。

```bash
cd firmware
./scripts/create-porting-assistant.sh
cd porting_assistant
idf.py reconfigure
SDKCONFIG_DEFAULTS="sdkconfig.defaults;managed_components/morsemicro__halow/configs/sdkconfig.defaults.seeed_xiao_esp32s3-seeed_xiao_mm6108" idf.py set-target esp32s3
idf.py menuconfig build
```

公式profileの基準値:

| Signal | XIAO ESP32S3 GPIO |
|---|---:|
| RESET_N | 1 |
| WAKE | 2 |
| BUSY | 5 |
| SPI SCK | 7 |
| SPI MOSI | 9 |
| SPI MISO | 8 |
| SPI CS | 4 |
| SPI IRQ | 3 |
| BCF | `bcf_fgh100mhaamd.bin` |

Porting assistantはmemory、timing、task、SPI、chip ID、firmware/BCF、bus throughput、BUSYを検査する。1項目でもFAILならAP/STAへ進まず、node交換でXIAO側/WM6180側を切り分ける。

## Gate 4: 2-node network

Porting assistantが3組でPASSした後にだけ、公式`softap`と`sta_connect`を別projectとして試す。

- 最初は1 AP + 1 STA
- WPA3-SAE credentialはrepositoryへcommitしない
- packet captureまたはconsole logでchannel、bandwidth、firmware、BCFを記録
- 3台目は既知正常node/交換試験用に残す
- AP/STA成功後にUDP echo、再起動復帰、8時間soakへ進む

## Stop条件

次の場合は通電/送信を止め、写真とlogをIssueへ添付する。

- rail短絡、異臭、過熱、USB切断の反復
- flash/PSRAM期待値不一致
- 公式profileでSPI chip IDを読めない
- BCF/FW mismatch
- RF fixtureまたは日本国内での適法な試験条件が未確保
