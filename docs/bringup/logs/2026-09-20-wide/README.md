# 2026-09-20 HaLow 2/4/8 MHz有限測定

[測定本文](../../halow-wide-performance-report-2026-09-20.md)の公開証拠。[metrics.json](metrics.json)はprivate原本からallowlistで作成し、caseごとの全stageの指定rate、実送信、受信unique、品質判定、実動作チャネル、ファームウェアSHA256、原本SHA256を保持する。SSID/PSK、raw SDKログ、ローカル絶対パスは含めない。

`status=FAIL`の初回4 MHz smokeと、共有チェックアウト切替によって修正前バイナリを再書き込みした4 MHz逆方向stage 0を保存した。60秒の3台復元確認は通信に成功したが、既存runnerの120秒/200 probe合格基準未満のためFAILのまま保持する。これらは最大速度の選択に使わない。

[final-state.json](final-state.json)には通常ファームウェアへ戻した3個体のflash照合、60秒・160秒・160秒再試験の順の判定、両STAのウォームアップ後計数、3台の終了marker、private原本SHA256を保存する。最初の160秒FAILと再試験PASSの両方を保持する。

8 MHz本測定2件の原reportに記録された`recorded_firmware_source_commit=043471f`は実バイナリのsourceではない。両バイナリはブランチ`bcfd652`のチェックアウト中（JST 23:54/23:56）にビルドされ、同SHA256でブランチ切替前の8 MHz smokeを通過した。切替は翌00:08:48であり、両端のboot markerは修正後固有の`entries=13`を示す。`verified_firmware_source_commit`はこれらの証拠から帰属した値で、原reportは書き換えていない。
