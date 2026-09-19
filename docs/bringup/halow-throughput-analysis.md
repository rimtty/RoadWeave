# Offline HaLow throughput analysis

`tools/halow_throughput_analyze.py` compares saved `report.json` files or the
published `metrics.json` bundle without serial hardware:

```powershell
python tools/halow_throughput_analyze.py .private/run-a/report.json .private/run-b/report.json --output .private/comparison.json
python tools/halow_throughput_analyze.py docs/bringup/logs/2026-09-19-throughput/metrics.json --events-dir docs/bringup/logs/2026-09-19-throughput
```

The JSON output gives each stage's target and actual TX rates, active and final
delivery, body goodput, and diagnostic flags. It reports the highest valid,
non-warmup observed goodput from a completed full case separately from the
three-repeat confirmed target; neither is a hardware ceiling. `--events-dir`
recomputes complete saved stage evidence from `CASE.events.jsonl` using the
runner's assessor and reports any discrepancy, including changed counters or
peer addresses. Adjacent comparison deltas require the same direction, width,
firmware source, and both binary hashes. Missing provenance blocks comparison.

A first failed search stage is never treated as a capacity bound: low-load loss
and confirmation variability may be nonmonotonic. Width is host metadata in
these reports; the captured events do not independently verify channel or
operating class.
