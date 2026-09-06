# opus component

libopus を ESP-IDF component としてビルドする。fixed-point、float API 無効、DRED/OSCE/Deep PLC 無効。

ソースは commit しない。使う前に取得する:

```bash
firmware/components/opus/fetch-upstream.sh   # xiph/opus v1.5.2 を ./upstream へ clone
```

Windows PowerShellではRepoルートから`.\firmware\components\opus\fetch-upstream.ps1`。
取得済みの場合もtag `v1.5.2`と未変更の作業ツリーを確認する。

ベンチ結果は [docs/bringup/opus-bench-2026-09-05.md](../../../docs/bringup/opus-bench-2026-09-05.md)。
license は BSD-3-Clause（`upstream/COPYING`）。
