# roadweave_reva (KiCad 8 project skeleton)

`docs/kicad-schematic-plan.md` の階層構成を KiCad 8 のファイルとして起こした **骨組みだけ**のプロジェクト（2026-09-06、スクリプト生成）。
root に 8 枚の階層シートを置き、各シートは空。回路は入っていない。

- この Mac に KiCad は入っていないため、**KiCad 8 で開けることは未確認**。開けない場合は root の `.kicad_sch` を KiCad 上で作り直し、
  シート名とファイル名だけ揃える（10 分）。
- 回路を書き始める前提: HAT 40 pin の実測、電源 rail review（`docs/power-budget.md` §5）、HC01 V2 の reference design 照合。
- net 名は `docs/kicad-schematic-plan.md` §2 のとおりに付ける。

生成スクリプトは残していない（1 回限り）。バックアップ（`*-backups/`, `*.kicad_sch-bak`）は `.gitignore` 済み。
