# 音声ネットワーク設計

更新日: 2026-09-05  
Protocol working name: RWP/0.1

## 1. 製品体験

RoadWeaveは「インターネットなしのDiscord風group voice」を目指す。ただし最初からfull-duplex multi-speakerにしない。

1. P0: 1対1 half-duplex IP-PTT
2. P1: group内で1人だけ発話するfloor-controlled PTT
3. P4: targeted PTT、個別mute/gain
4. 実測余力がある場合だけ最大2 active speakersのvoice room
5. P5: group recording

この順番ならAEC、複数decode/mix、RF競合を一度に持ち込まず、protocolの拡張余地だけ先に確保できる。

## 2. Network model

- HaLow AP/STA、local IPv4、internet gateway不要
- WPA3-SAEをlink layerの最低要件
- voiceはUDP、controlは最初UDP + idempotent message
- Coordinatorがgroup membershipとfloor leaseを管理
- Node IDは製造時IDとuser-visible callsignを分離
- Group secretからsession keyを派生するapplication encryptionはP4までに設計review

WPA3は無線linkを守るが、targeted PTTのprivacy boundaryをAP任せにしない。private/subgroup payloadは対象鍵で暗号化するか、対象peerだけへunicastする。

## 3. Traffic classes

| Class | 例 | Priority | Delivery |
|---|---|---:|---|
| C0 realtime control | PTT_START/END、floor grant | highest | repeated/idempotent |
| C1 voice | codec frames | high | loss tolerant、再送しない |
| C2 presence/position | peer/GPS 1 Hz | medium | newest wins |
| C3 config/log | settings、record index | low | reliable/task queue |
| C4 map/storage | PMTiles read/sync | lowest | audio deadline時にpause |

## 4. Packet header draft

Network byte order。wire formatはC structのpaddingへ依存させず、明示serializeする。

```text
magic          u16   0x5257 ("RW")
version        u8    0x01
type           u8    CONTROL / VOICE / POSITION / EVENT
codec          u8    VOICE時のpayload形式。0=IMA-ADPCM 16 kHz/20 ms、1=Opus。他typeは0
flags          u16   encrypted, start, end, fec, recording_hint...
header_len     u16
group_id       u32
sender_id      u32
target_type    u8    GROUP / NODE / SUBGROUP
target_id      u32   0 for GROUP
stream_id      u32   new value per PTT session
sequence       u32
capture_time   u32   monotonic ms modulo 2^32
payload_len    u16
payload        bytes
auth_tag       bytes optional by security profile
```

`codec`をheaderに持つことで、P0のIMA-ADPCMからP4のOpusへ移る際にprotocol versionを上げずに混在受信できる。受信側は未知のcodecを黙って捨て統計へ回す。

`stream_id + sequence`でreorder/lossを検出する。wall-clock未同期でも再生できるよう、音声はmonotonic capture timeを使う。録音時にGNSS UTCとの対応eventを別に残す。

## 5. Codec profiles

| Profile | Codec | Sample/frame | 目的 |
|---|---|---|---|
| P0 | IMA-ADPCM 64 kbps | 16 kHz / 20 ms | 実装が軽く、network/audio測定を分離 |
| Product speech | Opus 8〜12 kbps | 16 kHz / 40 ms候補 | airtimeと音質のbalance |
| High quality | Opus 16〜24 kbps | 16/24 kHz / 20〜40 ms | RF/CPU余力時のみ |

Opusはspeech向け低bitrateを選べるが、packet overhead、retransmission、PHY airtimeはcodec bitrateと同じではない。[Opus documentation](https://opus-codec.org/docs/)

## 6. Floor control

```text
IDLE
  -> PTT_DOWN: FLOOR_REQUEST(stream_id, target)
  -> GRANTED: local beep/indicator, begin voice
  -> PTT_UP: PTT_END, stop capture
  -> lease expiry/link loss: force stop and return IDLE
```

暫定timer:

- grant target: 100 ms以内
- lease: 750 ms、voice/control packetでrenew
- max continuous PTT: 120 s、その後warning/renew
- PTT_ENDを3回短間隔で送るが、受信側はidempotent

同時requestはCoordinator受信時刻とsender IDでdeterministicに決める。緊急優先は誤操作と権限設計が必要なためP0/P1では入れない。

## 7. Jitter/loss

- P0 jitter target: 20〜40 ms、adaptive上限80 ms
- late packetは再生せず統計へ
- voice UDP再送なし。将来FECはloss測定後
- sequence gapでPLC/短いsilenceを生成
- stream切替時はold streamを即flushし、speaker popを抑制

測定する指標:

- PTT downから相手の最初の可聴音まで
- capture-to-play p50/p95/p99
- packet loss、late loss、burst length
- jitter buffer underrun/overrun
- RF retry、RSSI/SNR、airtime

## 8. Individual mute/volume

mute/gainは受信側のlocal policyであり、相手へ状態を通知しなくても成立する。

```text
sender_id -> mute? -> decode -> per-user gain -> mix -> limiter -> speaker
```

- gainは0〜150%、UIは0〜100%を基本
- muteはpersistent setting、current speaker muteは即時
- master volumeとuser gainを分離
- limiterを最後段に置き、2 stream合成時のclippingを防ぐ

## 9. Targeted PTT

target modes:

- `GROUP`: 全員
- `SUBGROUP`: LEADERS、REARなど事前定義
- `NODE`: 指定1台

UIは運転中に深いmenuを要求しない。短押しは直前target、長押し/停車中操作でtarget変更、画面と音で`ALL / LEADERS / CAR-03`を確認する。

## 10. Recording roadmap

PCMを混ぜた1本のfileだけでなく、encoded frameとmetadataを保持する。

```text
session manifest
  group_id, participants, start UTC, device versions
audio records
  capture time, sender_id, stream_id, sequence, opus payload
events
  PTT, join/leave, mute policy, GPS time anchor
```

利点:

- 全員/特定senderだけを再生
- GPS logやdashcamと同期
- 後からmix levelを変更
- packet lossを記録として識別

初期状態OFF、REC中の常時indicator、参加者通知、storage暗号化、export/deleteを製品要件にする。

## 11. Group size

暫定製品目標は8台、protocol上限16台。これはHC01の接続上限を断定する値ではない。次を満たした最小値を正式specにする。

- 1 active speaker + 全node 1 Hz positionでp95 latency <=150 ms
- group churn中もfloor stateが壊れない
- Coordinator CPU/RAMとairtimeに30%以上の余裕
- Japan lineのairtime/Duty要件を満たす
