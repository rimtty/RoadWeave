# rwp component

RWP/0.1 wire format と floor control の pure C 実装。ESP-IDF に依存しないため、ホスト側で単体テストできる。

- `include/rwp.h` / `rwp.c`: 36 byte 固定ヘッダの serialize / parse、CONTROL payload、wrap-safe sequence 比較
- `include/floor.h` / `floor.c`: node 側 PTT 状態機械（IDLE / REQUESTING / TALKING / ENDING）と coordinator 側 floor table、同時 request の決定的調停
- `test_host/`: ASan/UBSan 付きのホストテスト

仕様は [docs/voice-networking.md](../../../docs/voice-networking.md) §4 と §6 を source of truth とする。

## ホストテスト

```bash
make -C firmware/components/rwp/test_host test
```

## 使い方（node 側）

```c
floor_node_t fn; floor_node_init(&fn, NULL, initial_stream_id);
uint32_t act = floor_node_step(&fn, FLOOR_EV_PTT_DOWN, now_ms);
if (act & FLOOR_ACT_SEND_REQUEST) send_control(RWP_CTRL_FLOOR_REQUEST, fn.stream_id);
// 10 ms 周期で FLOOR_EV_TICK、受信に応じて FLOOR_EV_GRANT / FLOOR_EV_DENY、
// voice packet 送信ごとに FLOOR_EV_VOICE_SENT を入れる。
```

時間は monotonic ms を注入する。wrap は考慮済み。
