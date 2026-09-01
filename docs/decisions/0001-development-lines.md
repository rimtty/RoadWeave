# ADR-0001: HC01 V2主系とMRF61_A日本副系を分ける

- Status: Accepted
- Date: 2026-09-02

## Context

RoadWeaveは長距離のHaLow IP-PTTを早く検証したい。一方、27 dBm級HC01 V2の利用条件と、日本国内で技適取得済みの13 dBm MRF61_Aでは、RF出力、電源、認証、antenna条件が異なる。両者はMM6108とSPI/SDIO hostという大枠を共有する。

## Decision

- P0-Aのreference bring-upはADR-0002に従い、XIAO ESP32S3 + Wio-WM6180で先行する。これは製品ラインの変更ではない。
- HC01 V2を機能・距離・電力評価のmain development lineとする。
- FGH100M-JとMRF61_AをJapan-compliance side lineとして比較し、protocolとapplicationを共通化する。
- RF module差分を`halow_port`、board config、regulatory profileへ隔離する。
- HC01 V2を日本向け量産品として既定採用しない。
- 日本国内でHC01 V2を評価する場合、適合確認前は通常の空中線送信を避け、適法な試験方法を選ぶ。
- Rev.Aで無理に同一footprint化せず、別schematic variantを許容する。

## Consequences

良い点:

- 高出力主系でPoCの技術上限を早く把握できる
- 日本向けの法令/antenna/Duty制約を後付けにしない
- voice、GPS、UI、recording protocolへの投資を共有できる

負担:

- BOM、RF test、carrier設計が2系統になる
- 同じMM6108でもBCF/FWとmodule電源差分の検証が必要
- 主系PoCの距離結果を日本版の性能として宣伝できない

## Revisit conditions

- HC01 V2の必要BCF/FWが再現可能に入手できない
- HC01 V2と同等の認証済みmoduleが主市場で合理的に調達できる
- MRF61_AのRFQ/供給条件がmain lineにも適する
- 対象地域の制度または認証条件が変わる
