# KiCad hardware

Rev.AのKiCad projectは、[schematic block plan](../../docs/kicad-schematic-plan.md)のreview後に作成する。

追加時に必要なもの:

- `.kicad_pro`, hierarchical `.kicad_sch`, `.kicad_pcb`
- custom symbol/footprintのsourceとlicense
- HC01 V2公式footprint/reference designとの照合記録
- stack-upと50 ohm impedance条件
- ERC/DRC report、schematic PDF、assembly drawing
- HC01 V2 mainとMRF61_A Japan variantの差分表

生成物やbackupは`.gitignore`に従い、release用fabrication outputだけをtagまたはrelease artifactとして保存する。

