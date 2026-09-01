# P0-A bring-up test log

このfileはtemplate。実測のたびに日付付きcopyを作るか、GitHub Issueへ同じ項目を記録する。住所、注文番号、個人連絡先、Wi-Fi passphraseは書かない。

## Test metadata

| Field | Value |
|---|---|
| Date/time | YYYY-MM-DD HH:MM JST |
| Operator |  |
| Git commit |  |
| ESP-IDF | v5.4.4 |
| Morse component | 2.11.2-esp32-2 |
| Host computer / OS |  |
| RF setup | disabled / 50 ohm load / conducted attenuator chain / shield box |
| Photos/log attachment |  |

## Node inventory

| Node | XIAO visual ID | WM6180 visual ID | USB port | Stack photo | Result |
|---|---|---|---|---|---|
| RW-N01 |  |  |  |  | pending |
| RW-N02 |  |  |  |  | pending |
| RW-N03 |  |  |  |  | pending |

## Gate 1: XIAO-only smoke

| Node | Flash MiB | PSRAM initialized | PSRAM MiB | `P0A_XIAO_SMOKE` | Replug boot | Result |
|---|---:|---|---:|---|---|---|
| RW-N01 |  |  |  |  |  | pending |
| RW-N02 |  |  |  |  |  | pending |
| RW-N03 |  |  |  |  |  | pending |

## Gate 2: unpowered stack

| Node | Orientation | GND continuity | 5V-GND short | 3V3-GND short | Result |
|---|---|---|---|---|---|
| RW-N01 |  |  |  |  | pending |
| RW-N02 |  |  |  |  | pending |
| RW-N03 |  |  |  |  | pending |

## Gate 3: official porting assistant

| Node | SPI | Chip ID | FW | BCF | Throughput | BUSY | Result |
|---|---|---|---|---|---|---|---|
| RW-N01 |  |  |  |  |  |  | pending |
| RW-N02 |  |  |  |  |  |  | pending |
| RW-N03 |  |  |  |  |  |  | pending |

## Gate 4: AP/STA

| Role | Node | Channel/bandwidth | RSSI/SNR | UDP echo | Reconnect | Result |
|---|---|---|---|---|---|---|
| AP |  |  |  |  |  | pending |
| STA |  |  |  |  |  | pending |
| Spare/cross-check |  |  |  |  |  | pending |

## Raw log

```text
Paste complete serial output here or link an Issue attachment.
```

## Decision

- [ ] Proceed to next gate
- [ ] Repeat with corrected setup
- [ ] Stop and open a blocking issue

Reason:

