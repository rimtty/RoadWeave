# 2026-09-19 HaLow throughput実測データ

[結果本文](../../p0-a-throughput-report-2026-09-19.md)の根拠データ。最終4条件と追加探索1件、保存した失敗3件、通常firmwareへの復元検査を含む。

- `metrics.json`: 各captureの元判定と、stageごとの独立再解析。送信実績、active/tail unique、UDP payload/body goodput、合格候補、最終無線停止証拠を含む。
- `manifest.json`: allowlistで抽出した公開eventsのSHA256と再解析一致結果。
- `firmware-manifest.json`: 実機に書いた保存binaryのSHA256を独立照合し、秘密を含まない設定だけを抽出。
- `host-source-manifest.json`: 初回未コミットsmokeの保存済みtool snapshotと、最終測定toolsのcommit/blob hash。
- `final-state.json`: 通常AA firmware復元後の3台通信・正常停止。throughput値とは別に判定し、個体・binary・flashとSTOP証拠を対応付ける。
- `*.events.jsonl`: 既知markerとfield、および試験用host操作記録。改行をLFへ固定しGit保存後もhashを一致させる。

SSID/PSK、private sdkconfig、SDKのrawログ、完全なローカルパスは公開しない。private原ログ・events・reportのhashをmetricsへ残す。

packet_bytes=1200はUDP payload全体、body_bytes=1184は試験headerを除く検証body。主goodputはactive unique bytes / max(TX DATA時間, RX active span)。END後のtailは主goodputに加算しない。loss率はactiveとfinalを分ける。

`late_recovered_stages_not_accepted_by_original_runner` はtimeout後のcleanup中にsummaryが届いたstageの参考再解析であり、元captureのFAILや未完了の3反復をPASSへ変更しない。
`usb_summary_delivery` はraw summaryのCRLF込みbytesと、device drain時間／hostでのACK→summary時間を区別して記録する。

初回stage0失敗と修正後5秒smokeの6packet欠落、最初の本測定のUSB summary timeoutを保持する。最大値は修正後の同一sourceによる本測定から判断し、短いsmokeや失敗captureを混ぜない。
