#!/usr/bin/env python3
"""Compare saved HaLow throughput reports without touching serial devices."""

from __future__ import annotations

import argparse
import json
import statistics
from pathlib import Path

import halow_throughput


def load_cases(paths: list[Path]) -> list[dict]:
    cases = []
    for path in paths:
        document = json.loads(path.read_text(encoding="utf-8"))
        rows = document.get("cases", [document])
        if not isinstance(rows, list):
            raise ValueError(f"{path}: cases must be a list")
        for index, row in enumerate(rows):
            if not isinstance(row, dict) or not isinstance(row.get("stages"), list):
                raise ValueError(f"{path}: case {index} lacks stages")
            cases.append({"name": row.get("case", path.stem), "source": str(path), "report": row})
    return cases


def provenance(report: dict) -> dict:
    manifest = report.get("manifest", {})
    devices = report.get("devices", manifest)
    return {
        "firmware_source": report.get("firmware_source", manifest.get("firmware_source_commit")),
        "ap_binary_sha256": devices.get("ap", {}).get("binary_sha256"),
        "sta_binary_sha256": devices.get("sta", {}).get("binary_sha256"),
        "host_commit": report.get("host_git", manifest.get("host_git", {})).get("commit"),
    }


def stage_summary(stage: dict) -> dict:
    target = stage.get("target_kbps")
    actual = stage.get("actual_tx_bps")
    active = stage.get("active_delivery")
    final = stage.get("final_delivery")
    tx = stage.get("tx") or {}
    rx = stage.get("rx") or {}
    flags = []
    if not stage.get("valid", False):
        flags.append("invalid_evidence")
    if isinstance(target, (int, float)) and target > 0 and isinstance(actual, (int, float)):
        if actual < target * 1000 * .95:
            flags.append("tx_rate_below_target")
    if isinstance(final, (int, float)) and final < .99:
        flags.append("rf_or_receive_loss")
    if isinstance(active, (int, float)) and isinstance(final, (int, float)) and final - active >= .01:
        flags.append("late_delivery")
    if tx.get("send_fail", 0) > 0:
        flags.append("send_failures")
    if rx.get("duplicate", 0) > 0:
        flags.append("duplicates")
    if isinstance(target, (int, float)) and 0 < target <= 256 and isinstance(final, (int, float)) and final < .99:
        flags.append("low_load_loss")
    return {
        "stage": stage.get("stage"), "phase": stage.get("phase"),
        "target_kbps": target, "actual_tx_kbps": actual / 1000 if isinstance(actual, (int, float)) else None,
        "rate_following": stage.get("rate_following"),
        "active_delivery": active, "final_delivery": final,
        "body_goodput_kbps": stage.get("active_body_goodput_bps", 0) / 1000
        if isinstance(stage.get("active_body_goodput_bps"), (int, float)) else None,
        "sent": tx.get("sent"), "received_active": rx.get("active_unique"),
        "received_final": rx.get("all_unique"),
        "sustainable": stage.get("sustainable"), "flags": flags,
    }


def summarize(case: dict) -> dict:
    report = case["report"]
    stages = [stage_summary(s) for s in report["stages"]]
    confirms = {}
    for stage in stages:
        if stage["phase"] == "confirm" and isinstance(stage["target_kbps"], (int, float)):
            confirms.setdefault(stage["target_kbps"], []).append(stage)
    variability = []
    for target, rows in sorted(confirms.items()):
        delivery = [r["final_delivery"] for r in rows if isinstance(r["final_delivery"], (int, float))]
        if len(delivery) >= 2:
            span = max(delivery) - min(delivery)
            variability.append({"target_kbps": target, "final_delivery_range": span,
                                "final_delivery_stdev": statistics.stdev(delivery),
                                "variable": span >= .01})
    confirmed = report.get("confirmed") or {}
    failed = [s for s in stages if s["phase"] in {"ramp", "lower_probe", "refine"}
              and s["sustainable"] is False]
    return {"name": case["name"], "source": case["source"],
            "status": report.get("status"), "mode": report.get("mode"),
            "direction": report.get("direction"), "bandwidth_mhz": report.get("bandwidth_mhz"),
            "bandwidth_is_host_metadata": True,
            "channel_opclass_verified_from_capture": False,
            "provenance": provenance(report),
            "confirmed_kbps": confirmed.get("target_kbps") if isinstance(confirmed, dict) else None,
            "stages": stages, "confirmation_variability": variability,
            "first_failed_search_stage": failed[0]["stage"] if failed else None,
            "first_failure_is_capacity_bound": False}


def reconcile_events(case: dict, events_dir: Path) -> list[str]:
    """Recompute saved stages from public events using the runner's assessor."""
    path = events_dir / f"{case['name']}.events.jsonl"
    if not path.is_file():
        return [f"missing events: {path}"]
    try:
        events = [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]
    except (OSError, ValueError) as exc:
        return [f"invalid events: {exc}"]
    errors = []
    for saved in case["report"]["stages"]:
        stage = saved.get("stage")
        hosts = [e for e in events if e.get("kind") == "host_stage" and e.get("stage") == stage]
        if len(hosts) != 1:
            errors.append(f"stage {stage}: expected one host_stage, got {len(hosts)}")
            continue
        host = hosts[0]
        for key, source in (("target_kbps", "rate_kbps"), ("requested_s", "duration_s"),
                            ("phase", "phase"), ("sender", "sender"), ("receiver", "receiver")):
            if saved.get(key) != host.get(source):
                errors.append(f"stage {stage}: saved {key} differs from host_stage")
        recalculated = halow_throughput.assess_stage(
            events, stage=stage, sender=host["sender"], receiver=host["receiver"],
            rate_kbps=host["rate_kbps"], requested_s=host["duration_s"], phase=host["phase"])
        for key in ("valid", "sustainable", "active_delivery", "final_delivery",
                    "actual_tx_bps", "active_body_goodput_bps", "rate_following"):
            a, b = saved.get(key), recalculated.get(key)
            if isinstance(a, (int, float)) and not isinstance(a, bool) and isinstance(b, (int, float)):
                matched = abs(a - b) <= max(1e-9, abs(b) * 1e-9)
            else:
                matched = a == b
            if not matched:
                errors.append(f"stage {stage}: saved {key} differs from events")
    return errors


def compare(cases: list[dict]) -> list[dict]:
    pairs = []
    for left, right in zip(cases, cases[1:]):
        differences = [key for key in ("direction", "bandwidth_mhz")
                       if left.get(key) != right.get(key)]
        for key in ("firmware_source", "ap_binary_sha256", "sta_binary_sha256"):
            a, b = left["provenance"].get(key), right["provenance"].get(key)
            if not a or not b:
                differences.append(f"{key}:missing")
            elif a != b:
                differences.append(key)
        a, b = left.get("confirmed_kbps"), right.get("confirmed_kbps")
        pairs.append({"from": left["name"], "to": right["name"],
                      "comparable": not differences, "differences": differences,
                      "confirmed_delta_kbps": b - a if not differences and a is not None and b is not None else None})
    return pairs


def analyze(paths: list[Path], events_dir: Path | None = None) -> dict:
    loaded = load_cases(paths)
    cases = [summarize(case) for case in loaded]
    if events_dir:
        for original, summary in zip(loaded, cases):
            summary["events_reconciliation_errors"] = reconcile_events(original, events_dir)
    return {"schema_version": 1, "cases": cases, "adjacent_comparisons": compare(cases)}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reports", type=Path, nargs="+", help="report.json or published metrics.json files")
    parser.add_argument("--output", type=Path, help="write JSON to this path (stdout by default)")
    parser.add_argument("--events-dir", type=Path, help="verify stages against CASE.events.jsonl files")
    args = parser.parse_args()
    try:
        result = analyze(args.reports, args.events_dir)
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        parser.error(str(exc))
    output = json.dumps(result, indent=2) + "\n"
    if args.output:
        args.output.write_text(output, encoding="utf-8")
    else:
        print(output, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
