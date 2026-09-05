# ADR-0003: HC01P V2 + HATをmain lineのbreakoutにし、Linux bridge試験を任意の副線にする

- Status: Proposed
- Date: 2026-09-05

## Context

Issue #1のコメントでは、main line候補としてHeltec **HC01P V2**（mini PCIe、MM6108、902–928 MHz）とHT-HC01P Raspberry Pi HAT（breakout用途）を推奨し、実際に3組を購入した。一方、README、ADR-0001、`docs/gpio-allocation.md`、`docs/kicad-schematic-plan.md`は**HT-HC01 V2**（1.27 mm stamp-hole LCCモジュール）を前提に書かれている。両者は同じMM6108と同じBCF系だが、形状・電源分配・入手経路・breakout方法が異なる。

HT-HC01データシートは、SDIO padがSPI modeを兼ねること（D1=SPI_INT、D0=MISO、CLK=SCK、CMD=MOSI、D3=CS）と、BUSY/RESET_N/WAKEが別padであることを示す。HC01P V2はこれをmini PCIe edgeへ引き出し、HATはさらにRaspberry Pi 40 pin headerへ引き出す。HATのRX/TX/GND headerはmoduleのUART（"Pending software support"）であり、host data pathではない。AT command/UART透過はHC02の機能で、HC01系にはない。

## Decision（提案）

1. 文書上の名称を「HC01 V2 family」に統一する。P0/P1のbreadboard段階は**HC01P V2 on HT-HC01P HAT**、Rev.A PCBは**HT-HC01 V2 LCC**を第一候補とし、両者を同一BCF/FW条件で扱えるかをporting assistantで確認する。
2. HATを使う前に、HATの回路図で40 pin header上のSDIO/SPI信号、BUSY、RESET_N、WAKE、5 V/3.3 V分配を確認し、`docs/gpio-allocation.md`にHAT pin→XIAO GPIOの対応表を追加する。HATの電源はXIAOの3V3ではなく5 Vから供給し、GNDを共通にする。
3. HC01P V2のBCFはHeltec指定の`bcf_mf08551.bin`を起点にし、WM6180の`bcf_fgh100mhaamd.bin`を流用しない。
4. Raspberry Pi（またはHeltec HD01）を用いたLinux bridge試験は、**RF link特性の計測と既製VoIP（Mumble等）による音声実用性の先行確認**に限定した任意の副線とする。ESP32側のP0-Bを代替しない。Raspberry Piが手元にない場合は実施しない。
5. 日本国内での空中送信を伴う試験は、ADR-0001/0002の安全gateに従い、conducted/shielded条件またはMRF61_A lineで行う。

## Consequences

良い点:

- 購入済みのHC01P V2 3組をmain lineの実機として位置付けられる
- Rev.A PCB用のLCC moduleとbreadboard用のmini PCIe moduleを同じBCF/FW条件で比較できる
- Linux bridge試験でESP32 firmwareの完成を待たずにRF特性を取れる

負担:

- HAT 40 pinの信号確認とjumper配線の信号品質（SPI 20〜40 MHz）に注意が必要
- HC01P V2とHT-HC01 V2の電源・RF pathの差を別gateとして記録する必要がある

## Revisit conditions

- HATの40 pin headerにSPI mode信号またはBUSY/RESET_N/WAKEが出ていない
- porting assistantでHC01P V2の`bcf_mf08551.bin`が通らない
- MRF61_A lineがmain lineに昇格する
