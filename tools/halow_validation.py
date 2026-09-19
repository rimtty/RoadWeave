#!/usr/bin/env python3
"""Analyze bounded HaLow link telemetry without opening serial ports."""

from __future__ import annotations

import argparse
import json
import math
import re
from pathlib import Path

MARKER = re.compile(r"(?:^|\s)(RW_LINK_[A-Z_]+)(?:=([^\s;]+))?(?:\s|$)")
FIELD = re.compile(r"([a-z][a-z0-9_]*)=([^\s;]+)")
ALLOWED = {
    "RW_LINK_AP_READY", "RW_LINK_RUN_START", "RW_LINK_SAMPLE",
    "RW_LINK_SUMMARY", "RW_LINK_RECOVERY", "RW_LINK_BOOT",
    "RW_LINK_ECHO", "RW_LINK_ECHO_MATCH", "RW_LINK_ECHO_DUPLICATE",
    "RW_LINK_TIMEOUT", "RW_LINK_UDP", "RW_LINK_RADIO_RESULT",
    "RW_LINK_DONE", "RW_LINK_CMD_ACK", "RW_LINK_STA_STATE",
    "RW_LINK_AP_STA_STATE", "RW_LINK_CONNECT_TIMEOUT",
    "RW_LINK_SOCKET_ERROR", "RW_LINK_SEND_FAIL",
    "RW_LINK_CHANNEL", "RW_LINK_RADIO_CONFIG", "RW_LINK_SCAN_TARGET",
    "RW_LINK_RUN_END", "RW_LINK_RADIO_SHUTDOWN",
    "RW_LINK_OUTAGE", "RW_LINK_AP_SERVICE", "RW_LINK_CMD_REJECT",
}
SAFE_FIELDS = {
    "role", "boot_id", "run_id", "reset_reason", "ip", "port", "window_s",
    "duration_s", "elapsed_s", "planned", "sent", "received", "lost",
    "duplicates", "late", "seq", "rtt_us", "rtt_mean_us", "rtt_max_us",
    "rtt_p50_us", "rtt_p95_us", "rtt_p99_us", "max_us", "offered_bps",
    "useful_bps", "heap_free", "heap_min", "reconnects", "downtime_ms",
    "reason", "command", "payload_bytes", "interval_ms", "peer", "status",
    "value", "state", "aid", "errno", "bits", "result", "id",
    "freq_hz", "bw_mhz", "opclass", "channel", "country", "max_tx_dbm", "rssi_dbm",
    "elapsed_ms", "send_fail", "skipped", "invalid", "echo_id1", "echo_id2",
    "rc_sent_start", "rc_sent_end", "rc_success_start", "rc_success_end",
    "snr_db", "max_probes", "max_stas", "phase",
    "rtt_hist_bin_us", "execution_ok", "quality_gate",
}


def parse_line(line: str) -> dict | None:
    """Extract known telemetry only; arbitrary serial text stays private."""
    match = MARKER.search(line)
    if not match or match.group(1) not in ALLOWED:
        return None
    fields = {k: v for k, v in FIELD.findall(line[match.start():]) if k in SAFE_FIELDS}
    if match.group(2) is not None:
        fields["value"] = match.group(2)
    return {"marker": match.group(1), "fields": fields}


def number(fields: dict, key: str) -> int | float | None:
    try:
        n = float(fields[key])
    except (KeyError, TypeError, ValueError):
        return None
    return n if math.isfinite(n) and n >= 0 else None


def analyze(events: list[dict], expected_roles=("ap", "sta")) -> dict:
    errors: list[str] = []
    roles = {}
    for role in expected_roles:
        row = [e for e in events if e.get("role") == role and e.get("kind") == "firmware"]
        starts = [e for e in row if e.get("marker") == "RW_LINK_RUN_START"]
        summaries = [e for e in row if e.get("marker") == "RW_LINK_SUMMARY"]
        samples = [e for e in row if e.get("marker") == "RW_LINK_SAMPLE"]
        echoes = [e for e in row if e.get("marker") in {"RW_LINK_ECHO", "RW_LINK_ECHO_MATCH"}]
        duplicates = [e for e in row if e.get("marker") == "RW_LINK_ECHO_DUPLICATE"]
        boots = [e for e in row if e.get("marker") == "RW_LINK_BOOT"]
        recoveries = [e for e in row if e.get("marker") == "RW_LINK_RECOVERY"]
        if not starts:
            errors.append(f"{role}: missing RW_LINK_RUN_START")
        if not summaries:
            errors.append(f"{role}: missing RW_LINK_SUMMARY")
        if starts and summaries and summaries[-1].get("monotonic_s", -1) < starts[-1].get("monotonic_s", 0):
            errors.append(f"{role}: final run has no summary")
        summary = summaries[-1].get("fields", {}) if summaries else {}
        if starts:
            unexpected = [b for b in boots if b.get("monotonic_s", -1) > starts[0].get("monotonic_s", 0)]
            software_faults = [f for f in events if f.get("kind") == "fault" and f.get("role") == role
                               and f.get("action") == "software_reset"]
            if len(unexpected) != len(software_faults):
                errors.append(f"{role}: unexpected or missing post-start boot count")
        sent, received, lost = (number(summary, k) for k in ("sent", "received", "lost"))
        if role == "sta":
            if sent is None or received is None or lost is None:
                errors.append("sta: summary lacks valid sent/received/lost counts")
            elif sent <= 0 or received <= 0 or received > sent or lost != sent - received:
                errors.append("sta: summary counters are inconsistent or have no valid echoes")
        previous = None
        counter_keys = ("sent", "received", "lost") if role == "sta" else ("echo_id1", "echo_id2")
        for sample in samples:
            counts = tuple(number(sample.get("fields", {}), k) for k in counter_keys)
            if any(n is None for n in counts):
                errors.append(f"{role}: sample has missing or invalid counters")
                break
            if previous and any(current < older for current, older in zip(counts, previous)):
                # A new boot/run resets cumulative counters.
                if not any(start.get("monotonic_s", -1) > previous_time and
                           start.get("monotonic_s", -1) <= sample.get("monotonic_s", -1)
                           for start in starts):
                    errors.append(f"{role}: sample counters decreased within one run")
                    break
            previous = counts
            previous_time = sample.get("monotonic_s", -1)
        final_start_time = starts[-1].get("monotonic_s", -1) if starts else -1
        final_echoes = [e for e in echoes if e.get("monotonic_s", -1) >= final_start_time]
        final_sequences = [e.get("fields", {}).get("seq") for e in final_echoes]
        final_sequences = [s for s in final_sequences if s is not None]
        if role == "sta" and received is not None and len(final_sequences) != received:
            errors.append("sta: final run logged valid echo count differs from summary received")
        seqs = [e.get("fields", {}).get("seq") for e in echoes]
        seqs = [s for s in seqs if s is not None]
        if len(final_sequences) != len(set(final_sequences)):
            errors.append(f"{role}: repeated echo match sequence in final run")
        roles[role] = {
            "run_start": starts[-1].get("fields", {}) if starts else None,
            "run_count": len(starts),
            "summary": summary if summaries else None,
            "sample_count": len(samples), "echo_matches_logged": len(seqs),
            "duplicates_logged": len(duplicates),
            "recovery_windows_ms": [number(e.get("fields", {}), "downtime_ms") for e in recoveries],
            "heap_free_samples": [number(e.get("fields", {}), "heap_free") for e in samples],
            "boots": [e.get("fields", {}) for e in boots],
            "cold_boot_counted": 0,
        }
        final_start = starts[-1].get("monotonic_s", -1) if starts else -1
        final_events = [e for e in row if e.get("monotonic_s", -1) >= final_start]
        markers = {e.get("marker") for e in final_events}
        for required in ("RW_LINK_RUN_END", "RW_LINK_RADIO_RESULT", "RW_LINK_RADIO_SHUTDOWN", "RW_LINK_DONE"):
            if required not in markers:
                errors.append(f"{role}: missing final {required}")
        results = [e.get("fields", {}).get("value") for e in final_events
                   if e.get("marker") == "RW_LINK_RADIO_RESULT"]
        if not results or results[-1] != "PASS":
            errors.append(f"{role}: final radio result is not PASS")
        for e in final_events:
            if e.get("marker") in {"RW_LINK_SOCKET_ERROR", "RW_LINK_CONNECT_TIMEOUT", "RW_LINK_CMD_REJECT"}:
                errors.append(f"{role}: firmware reported {e['marker']}")
    faults = [e for e in events if e.get("kind") == "fault"]
    ap_ready = [e for e in events if e.get("role") == "ap" and e.get("marker") == "RW_LINK_AP_READY"]
    sta_start = [e for e in events if e.get("role") == "sta" and e.get("marker") == "RW_LINK_RUN_START"]
    if not ap_ready or not sta_start or ap_ready[0].get("monotonic_s", 0) > sta_start[0].get("monotonic_s", 0):
        errors.append("STA run did not follow a recorded AP_READY gate")
    for fault in faults:
        role = fault.get("role")
        if fault.get("action") not in {"software_reset", "ap_off_10s"}:
            errors.append(f"{role}: unsupported fault action")
        if not fault.get("acknowledged"):
            errors.append(f"{role}: fault command was not acknowledged")
        fault_index = faults.index(fault)
        next_fault_time = faults[fault_index + 1].get("monotonic_s", float("inf")) if fault_index + 1 < len(faults) else float("inf")
        after = [e for e in events if e.get("role") == role and e.get("kind") == "firmware"
                 and fault.get("monotonic_s", float("inf")) < e.get("monotonic_s", -1) < next_fault_time]
        needed = "RW_LINK_BOOT" if fault.get("action") == "software_reset" else "RW_LINK_AP_READY"
        if not any(e.get("marker") == needed for e in after):
            errors.append(f"{role}: no {needed} after injected fault")
        if fault.get("action") == "software_reset":
            boot = next((e for e in after if e.get("marker") == "RW_LINK_BOOT"), None)
            reason = (boot or {}).get("fields", {}).get("reset_reason", "").upper()
            if reason not in {"3", "SW", "SOFTWARE", "ESP_RST_SW"}:
                errors.append(f"{role}: boot after software reset did not report software reset")
            if sum(e.get("marker") == "RW_LINK_BOOT" for e in after) != 1:
                errors.append(f"{role}: expected exactly one boot after software reset")
        elif any(e.get("marker") == "RW_LINK_BOOT" for e in after):
            errors.append(f"{role}: AP service outage unexpectedly rebooted")
        # The STA reports link recovery even when the AP was the fault target.
        if role == "ap":
            recovery_after = [e for e in events if e.get("role") == "sta" and
                              e.get("kind") == "firmware" and
                              fault.get("monotonic_s", float("inf")) < e.get("monotonic_s", -1) < next_fault_time]
            if not any(e.get("marker") == "RW_LINK_RECOVERY" for e in recovery_after):
                errors.append("sta: no recovery marker after AP fault")
    runner_errors = [e.get("message", "runner failure") for e in events if e.get("kind") == "error"]
    errors.extend(runner_errors)
    errors.extend(f"{e.get('role', '?')}: raw serial panic/watchdog marker {e.get('code', '?')}"
                  for e in events if e.get("kind") == "panic")
    return {"status": "PASS" if not errors else "FAIL", "errors": errors,
            "roles": roles, "faults": faults, "event_count": len(events), "cold_boot_counted": 0}


def apply_acceptance(report: dict, events: list[dict], *, min_sent=200,
                     min_delivery=0.99, min_duration_s=120,
                     recovery_limit_s=60, consecutive=20) -> dict:
    """Apply the P0-A finite-run gates to an analyzed capture."""
    errors = report["errors"]
    sta = report["roles"].get("sta", {})
    summary = sta.get("summary") or {}
    sent, received = number(summary, "sent"), number(summary, "received")
    faults = report["faults"]
    if not faults:
        samples = [e for e in events if e.get("role") == "sta" and e.get("marker") == "RW_LINK_SAMPLE"]
        warmup = next((e for e in samples if number(e.get("fields", {}), "elapsed_ms") is not None
                       and number(e.get("fields", {}), "elapsed_ms") >= 10000), None)
        ends = [e for e in events if e.get("role") == "sta" and e.get("marker") == "RW_LINK_SUMMARY"]
        if warmup is None:
            errors.append("baseline: missing 10s warmup sample")
        else:
            warmup_sent = number(warmup.get("fields", {}), "sent")
            warmup_received = number(warmup.get("fields", {}), "received")
            post_sent = sent - warmup_sent if sent is not None and warmup_sent is not None else None
            post_received = received - warmup_received if received is not None and warmup_received is not None else None
            report["post_warmup"] = {"sent": post_sent, "received": post_received,
                                      "warmup_elapsed_ms": number(warmup.get("fields", {}), "elapsed_ms"),
                                      "duration_s": ends[-1].get("monotonic_s", 0) - warmup.get("monotonic_s", 0) if ends else None}
            if post_sent is None or post_sent < min_sent:
                errors.append(f"baseline: fewer than {min_sent} successfully sent probes after warmup")
            if post_sent is None or post_received is None or post_sent <= 0 or post_received / post_sent < min_delivery:
                errors.append(f"baseline: post-warmup valid echo delivery below {min_delivery:.1%}")
            if not ends or ends[-1].get("monotonic_s", 0) - warmup.get("monotonic_s", 0) < min_duration_s:
                errors.append(f"baseline: post-warmup observation shorter than {min_duration_s}s")
    else:
        for index, fault in enumerate(faults):
            start = fault.get("monotonic_s", 0)
            end = faults[index + 1].get("monotonic_s", float("inf")) if index + 1 < len(faults) else float("inf")
            role = fault.get("role")
            if role == "sta":
                anchor_marker, anchor_role = "RW_LINK_BOOT", "sta"
            else:
                anchor_marker, anchor_role = "RW_LINK_AP_READY", "ap"
            anchors = [e for e in events if start < e.get("monotonic_s", -1) < end and
                       e.get("role") == anchor_role and e.get("marker") == anchor_marker]
            if not anchors:
                errors.append(f"fault {index + 1}: missing recovery timing anchor {anchor_marker}")
                continue
            anchor = anchors[0].get("monotonic_s", 0)
            fault["anchor_monotonic_s"] = anchor
            echoes = [e for e in events if anchor < e.get("monotonic_s", -1) < end and
                      e.get("role") == "sta" and e.get("marker") == "RW_LINK_ECHO_MATCH"]
            if not echoes or echoes[0]["monotonic_s"] - anchor > recovery_limit_s:
                errors.append(f"fault {index + 1}: valid echo did not resume within {recovery_limit_s}s")
                continue
            fault["first_echo_monotonic_s"] = echoes[0]["monotonic_s"]
            fault["recovery_s"] = echoes[0]["monotonic_s"] - anchor
            sequence = [number(e.get("fields", {}), "seq") for e in echoes]
            streak = best = 0
            last = None
            for n in sequence:
                streak = streak + 1 if n is not None and (last is None or n == last + 1) else 1
                best = max(best, streak)
                last = n
            if best < consecutive:
                errors.append(f"fault {index + 1}: fewer than {consecutive} consecutive exact echoes")
            fault["longest_exact_echo_streak"] = best
    report["status"] = "PASS" if not errors else "FAIL"
    return report


def load_events(path: Path) -> list[dict]:
    events = []
    for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        try:
            event = json.loads(line)
        except json.JSONDecodeError as exc:
            raise ValueError(f"{path}:{line_number}: invalid or truncated JSONL event") from exc
        if not isinstance(event, dict):
            raise ValueError(f"{path}:{line_number}: expected object")
        events.append(event)
    return events


def markdown(report: dict) -> str:
    lines = ["# HaLow finite validation", "", f"Result: **{report['status']}**", "",
             "| Role | Sent | Valid echoes | Loss | Duplicates | Samples | Recovery windows (ms) |",
             "|---|---:|---:|---:|---:|---:|---|"]
    for role, entry in report["roles"].items():
        s = entry["summary"] or {}
        windows = ", ".join(str(v) for v in entry["recovery_windows_ms"] if v is not None) or "—"
        lines.append(f"| {role} | {s.get('sent', '—')} | {s.get('received', '—')} | "
                     f"{s.get('lost', '—')} | {s.get('duplicates', '—')} | "
                     f"{entry['sample_count']} | {windows} |")
    sta = report["roles"].get("sta", {}).get("summary") or {}
    post = report.get("post_warmup")
    if post:
        lines.extend(["", f"After 10s warmup: {post.get('sent')} sent, {post.get('received')} exact replies "
                      f"over {post.get('duration_s', '—')}s."])
    if sta:
        lines.extend(["", f"STA RTT p50/p95/max: {sta.get('rtt_p50_us', '—')}/"
                      f"{sta.get('rtt_p95_us', '—')}/{sta.get('rtt_max_us', '—')} µs; "
                      f"offered/useful: {sta.get('offered_bps', '—')}/"
                      f"{sta.get('useful_bps', '—')} bps; RSSI: {sta.get('rssi_dbm', '—')} dBm.",
                      f"STA send failures/skipped/invalid: {sta.get('send_fail', '—')}/"
                      f"{sta.get('skipped', '—')}/{sta.get('invalid', '—')}; "
                      f"heap free/min: {sta.get('heap_free', '—')}/{sta.get('heap_min', '—')} bytes; "
                      f"reconnects: {sta.get('reconnects', '—')}."])
    lines.extend(["", "Cold boots credited: **0**. A serial or firmware reset is not a power cycle.", ""])
    warning_counts = report.get("manifest", {}).get("warning_counts")
    if warning_counts:
        lines.extend(["Raw serial warning counts by role and boot (locations in report.json):", ""])
        for role, counts in warning_counts.items():
            lines.append(f"- {role}: Address base set failed={counts.get('address_base_set_failed', 0)}; "
                         f"Unknown TLV={counts.get('unknown_tlv', 0)}")
        lines.extend(["", "Address-base failure is a propagated transport error; unknown TLV is skipped by the parser. Review raw context before classifying either warning.", ""])
    if report["faults"]:
        lines.extend(["Injected faults:", ""])
        for f in report["faults"]:
            lines.append(f"- {f.get('iso_time', '?')} {f.get('role', '?')}: "
                         f"{f.get('action', '?')}, acknowledged={f.get('acknowledged', False)}, "
                         f"first exact echo after anchor={f.get('recovery_s', '—')} s, "
                         f"longest consecutive streak={f.get('longest_exact_echo_streak', '—')}")
        lines.append("")
    if report["errors"]:
        lines.extend(["Failures:", ""])
        lines.extend(f"- {e}" for e in report["errors"])
        lines.append("")
    return "\n".join(lines)


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("events", type=Path)
    p.add_argument("--json", type=Path, required=True)
    p.add_argument("--markdown", type=Path, required=True)
    args = p.parse_args()
    try:
        events = load_events(args.events)
        report = apply_acceptance(analyze(events), events)
    except (OSError, ValueError) as exc:
        report = {"status": "FAIL", "errors": [str(exc)], "roles": {}, "faults": [],
                  "event_count": 0, "cold_boot_counted": 0}
    args.json.parent.mkdir(parents=True, exist_ok=True)
    args.markdown.parent.mkdir(parents=True, exist_ok=True)
    args.json.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    args.markdown.write_text(markdown(report), encoding="utf-8")
    print(report["status"])
    return 0 if report["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
