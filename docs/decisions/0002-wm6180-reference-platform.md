# ADR-0002: WM6180をP0 reference platformにする

- Status: Accepted
- Date: 2026-09-02

## Context

HC01 V2はRoadWeave製品主系の候補だが、ESP32-S3との接続、BCF/FW、電源、RF fixtureを同時に自作すると、失敗時に原因を切り分けにくい。Wio-WM6180はXIAO headerへ直接挿せ、Morse Micro公式componentにXIAO ESP32S3 + MM6108用のpin/BCF profileがある。

P0ではXIAO ESP32S3とWio-WM6180を3組使用できる。これにより2-node試験と、交換診断用の既知正常nodeを同時に持てる。

## Decision

- P0-Aの最初のreference bring-upはXIAO ESP32S3 + Wio-WM6180で行う。
- 公式board profileと固定component versionを使い、独自GPIO/BCF調整より先にporting assistantを通す。
- 最初のflashはXIAO単体診断とし、HaLowを初期化しない。
- Wio-WM6180は開発referenceであり、そのままRoadWeave量産BOMへ採用する前提にしない。
- HC01 V2を製品主開発ラインとして維持する。
- 日本向けはFGH100M-JおよびMRF61_Aを適合・供給・性能の比較対象にし、北米向けWM6180の結果を日本版の適合根拠にしない。

## Consequences

良い点:

- はんだ付けや自作carrierなしでhost/software bring-upを始められる
- 公式pin/BCFとの差異を排除し、故障交換で原因を切り分けられる
- voice/network softwareをHC01 carrier完成前に進められる

負担:

- P0 referenceと製品候補で回路・電源・RF出力が異なる
- WM6180での成功はHC01 V2固有の5 V PA、BCF、27 dBm動作を証明しない
- FGH100M-Hの周波数/認証条件により、日本国内のRF試験方法が制限される

## Revisit conditions

- 公式XIAO/MM6108 profileでporting assistantを再現できない
- HC01 V2 carrierが完成し、同等以上の再現性で起動できる
- 日本適合moduleを同じ開発速度・調達条件で入手できる
