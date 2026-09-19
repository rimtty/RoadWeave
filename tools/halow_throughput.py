#!/usr/bin/env python3
"""Parse and assess bounded one-way HaLow UDP throughput stages offline."""

from __future__ import annotations

import math
import re

MARKER = re.compile(r"RW_TPUT_[A-Z_]+")
FIELD = re.compile(r"([a-z][a-z0-9_]*)=([^\s;]+)")
ALLOWED = {
    "RW_TPUT_SESSION_START", "RW_TPUT_SESSION_END", "RW_TPUT_READY",
    "RW_TPUT_ARM_ACK", "RW_TPUT_RX_READY",
    "RW_TPUT_RX_START", "RW_TPUT_RX_SAMPLE", "RW_TPUT_RX_END",
    "RW_TPUT_RX_DRAIN",
    "RW_TPUT_RX_SUMMARY", "RW_TPUT_SEND_ACK", "RW_TPUT_TX_START",
    "RW_TPUT_TX_SAMPLE", "RW_TPUT_TX_END", "RW_TPUT_TX_SUMMARY",
    "RW_TPUT_CMD_ACK", "RW_TPUT_STAGE_ABORT", "RW_TPUT_SESSION_ABORT",
    "RW_TPUT_SOCKET_ERROR", "RW_TPUT_SEND_ERROR", "RW_TPUT_CMD_REJECT",
}
SAFE_FIELDS = {
    "stage", "role", "id", "ip", "port", "peer", "boot_id", "reset_reason",
    "session_s", "sdk_log", "stages", "all_stages_ok", "stopped", "t_us",
    "duration_s", "requested_ms", "duration_us", "packet_bytes", "body_bytes",
    "rate_kbps", "attempts", "sent", "send_fail", "sent_udp_bytes",
    "sent_body_bytes", "active_body_bytes", "all_body_bytes",
    "active_unique", "late_unique", "drain_unique", "all_unique", "duplicate",
    "out_of_order", "invalid", "wrong_stage", "wrong_peer", "overflow",
    "start_seen", "end_seen", "end_sent", "drain_sent", "drain_command",
    "count_match", "start_acked", "end_acked", "aborted", "start_us",
    "end_us", "first_active_us", "last_active_us", "active_span_us",
    "drain_start_us", "drain_end_us", "elapsed_us", "elapsed_ms", "error",
    "reason", "command", "errno", "phase", "value",
}


def parse_lines(line: str) -> list[dict]:
    """Accept known markers even when driver text and UART records coalesce."""
    matches = list(MARKER.finditer(line))
    parsed = []
    for index, match in enumerate(matches):
        marker = match.group(0)
        if marker not in ALLOWED:
            continue
        end = matches[index + 1].start() if index + 1 < len(matches) else len(line)
        segment = line[match.end():end]
        fields = {k: v for k, v in FIELD.findall(segment) if k in SAFE_FIELDS}
        value = re.match(r"=([^\s;]+)", segment)
        if value:
            fields["value"] = value.group(1)
        parsed.append({"marker": marker, "fields": fields})
    return parsed


def count(fields: dict, name: str) -> int | None:
    """Require exact nonnegative integer counters, never rounded floats."""
    value = fields.get(name)
    if value is None or not re.fullmatch(r"\d+", str(value)):
        return None
    return int(value)


def truth(fields: dict, name: str) -> bool | None:
    value = str(fields.get(name, "")).lower()
    if value in {"1", "true", "yes"}:
        return True
    if value in {"0", "false", "no"}:
        return False
    return None


def stage_events(events: list[dict], stage: int) -> list[dict]:
    return [event for event in events if event.get("kind") == "firmware" and
            count(event.get("fields", {}), "stage") == stage]


def assess_stage(events: list[dict], *, stage: int, sender: str, receiver: str,
                 rate_kbps: int, requested_s: int, phase: str,
                 sender_ip: str | None = None, receiver_ip: str | None = None) -> dict:
    """Reconcile both radios' final counters and device-clock measurement span.

    ``valid`` is structural evidence; ``sustainable`` additionally applies the
    paced-rate and 99% active/final delivery gates. Unpaced stages are only
    saturation observations, never sustainable candidates.
    """
    rows = stage_events(events, stage)
    errors = []

    def one(role, marker):
        matches = [e for e in rows if e.get("role") == role and e.get("marker") == marker]
        if len(matches) != 1:
            errors.append(f"stage {stage}: expected one {role} {marker}, got {len(matches)}")
            return {}
        return matches[0].get("fields", {})

    arm = one(receiver, "RW_TPUT_ARM_ACK")
    send = one(sender, "RW_TPUT_SEND_ACK")
    for role, marker in ((receiver, "RW_TPUT_RX_READY"),
                         (receiver, "RW_TPUT_RX_START"),
                         (sender, "RW_TPUT_TX_START"),
                         (sender, "RW_TPUT_TX_END"),
                         (receiver, "RW_TPUT_RX_DRAIN")):
        one(role, marker)
    tx = one(sender, "RW_TPUT_TX_SUMMARY")
    rx = one(receiver, "RW_TPUT_RX_SUMMARY")
    for event in rows:
        if event.get("marker") in {"RW_TPUT_STAGE_ABORT", "RW_TPUT_SOCKET_ERROR", "RW_TPUT_CMD_REJECT"}:
            errors.append(f"stage {stage}: firmware {event['marker']}")

    tx_keys = ("packet_bytes", "body_bytes", "rate_kbps", "requested_ms",
               "attempts", "sent", "send_fail", "sent_udp_bytes",
               "sent_body_bytes", "start_us", "end_us", "duration_us")
    rx_keys = ("packet_bytes", "body_bytes", "active_unique", "late_unique",
               "drain_unique", "all_unique", "duplicate", "invalid",
               "wrong_stage", "wrong_peer", "overflow", "first_active_us",
               "last_active_us", "active_body_bytes", "all_body_bytes",
               "end_sent", "drain_sent", "active_span_us", "drain_start_us",
               "drain_end_us")
    t = {key: count(tx, key) for key in tx_keys}
    r = {key: count(rx, key) for key in rx_keys}
    for name, values in (("TX", t), ("RX", r)):
        absent = [key for key, value in values.items() if value is None]
        if absent:
            errors.append(f"stage {stage}: {name} summary missing/invalid {','.join(absent)}")
    for role, fields in (("TX", tx), ("RX", rx)):
        if truth(fields, "aborted") is not False:
            errors.append(f"stage {stage}: {role} aborted state is not false")
    if truth(tx, "start_acked") is not True or truth(tx, "end_acked") is not True:
        errors.append(f"stage {stage}: missing START/END acknowledgment")
    for key in ("start_seen", "end_seen", "drain_command", "count_match"):
        if truth(rx, key) is not True:
            errors.append(f"stage {stage}: receiver {key} is not true")

    packet = t["packet_bytes"]
    body = t["body_bytes"]
    if packet != 1200 or body != 1184 or r["packet_bytes"] != packet or r["body_bytes"] != body:
        errors.append(f"stage {stage}: packet/body size differs across devices or from 1200/1184")
    sent, active, all_unique = t["sent"], r["active_unique"], r["all_unique"]
    if None not in (sent, active, all_unique, t["attempts"], t["send_fail"],
                    r["late_unique"], r["drain_unique"], r["end_sent"], r["drain_sent"]):
        if sent <= 0 or sent + t["send_fail"] != t["attempts"]:
            errors.append(f"stage {stage}: sender counts are inconsistent")
        if not (0 <= active <= all_unique <= sent):
            errors.append(f"stage {stage}: receiver unique count exceeds successful sends")
        if active + r["late_unique"] != all_unique or r["drain_unique"] > r["late_unique"]:
            errors.append(f"stage {stage}: active/tail unique accounting differs")
        if r["end_sent"] != sent or r["drain_sent"] != sent:
            errors.append(f"stage {stage}: END/DRAIN sent count differs from TX summary")
        if (t["sent_udp_bytes"] != sent * 1200 or t["sent_body_bytes"] != sent * 1184 or
                r["active_body_bytes"] != active * 1184 or
                r["all_body_bytes"] != all_unique * 1184):
            errors.append(f"stage {stage}: sender/receiver byte counters disagree")
    if any(r[key] not in (0, None) for key in ("invalid", "wrong_stage", "wrong_peer", "overflow")):
        errors.append(f"stage {stage}: invalid/wrong-stage/wrong-peer/overflow packets")
    if t["requested_ms"] != requested_s * 1000:
        errors.append(f"stage {stage}: TX requested duration differs from host stage")
    if t["rate_kbps"] != rate_kbps:
        errors.append(f"stage {stage}: TX configured rate differs from host stage")
    for role, fields, expected_peer in (("ARM", arm, sender_ip), ("SEND", send, receiver_ip)):
        if (count(fields, "duration_s") != requested_s or
                count(fields, "packet_bytes") != 1200 or not fields.get("peer")):
            errors.append(f"stage {stage}: {role} acknowledgment parameters are incomplete")
        if expected_peer and fields.get("peer") != expected_peer:
            errors.append(f"stage {stage}: {role} peer address differs from ready device")
    if count(send, "rate_kbps") != rate_kbps:
        errors.append(f"stage {stage}: SEND acknowledgment rate differs from host stage")
    if arm.get("peer") and send.get("peer") and arm.get("peer") == send.get("peer"):
        errors.append(f"stage {stage}: ARM/SEND peer addresses are identical")
    elapsed = t["duration_us"]
    first, last = r["first_active_us"], r["last_active_us"]
    if elapsed is None or elapsed <= 0 or elapsed < requested_s * 990_000:
        errors.append(f"stage {stage}: DATA window shorter than 99% of request")
    if first is None or last is None or last < first:
        errors.append(f"stage {stage}: invalid receiver device-clock span")
    if t["start_us"] is None or t["end_us"] is None or t["end_us"] - t["start_us"] != elapsed:
        errors.append(f"stage {stage}: TX device-clock duration differs from endpoints")
    if first is not None and last is not None and r["active_span_us"] != last - first:
        errors.append(f"stage {stage}: RX active span differs from device-clock endpoints")
    if active and (first is None or last is None or first <= 0 or last <= 0):
        errors.append(f"stage {stage}: received packets lack positive device timestamps")
    if (r["drain_start_us"] is None or r["drain_end_us"] is None or
            r["drain_end_us"] - r["drain_start_us"] < 3_000_000):
        errors.append(f"stage {stage}: RX drain shorter than 3 s")

    measured_s = max(elapsed / 1_000_000, (last - first) / 1_000_000) if (
        elapsed is not None and elapsed > 0 and first is not None and last is not None and last >= first) else None
    active_delivery = active / sent if sent and active is not None and active <= sent else None
    final_delivery = all_unique / sent if sent and all_unique is not None and all_unique <= sent else None
    actual_tx_bps = sent * 1200 * 8 / (elapsed / 1_000_000) if sent and elapsed and elapsed > 0 else None
    active_payload_bps = active * 1200 * 8 / measured_s if active is not None and measured_s else None
    active_body_bps = active * 1184 * 8 / measured_s if active is not None and measured_s else None
    paced = rate_kbps > 0
    rate_following = actual_tx_bps / (rate_kbps * 1000) if paced and actual_tx_bps is not None else None
    valid = not errors
    sustainable = valid and paced and active_delivery is not None and active_delivery >= .99 and (
        final_delivery is not None and final_delivery >= .99) and (
        rate_following is not None and rate_following >= .95)
    return {
        "stage": stage, "phase": phase, "sender": sender, "receiver": receiver,
        "target_kbps": rate_kbps, "requested_s": requested_s,
        "valid": valid, "sustainable": sustainable, "errors": errors,
        "tx": t, "rx": r, "tx_data_s": elapsed / 1_000_000 if elapsed else None,
        "rx_active_span_s": (last - first) / 1_000_000 if first is not None and last is not None and last >= first else None,
        "goodput_denominator_s": measured_s,
        "active_delivery": active_delivery, "final_delivery": final_delivery,
        "actual_tx_bps": actual_tx_bps, "rate_following": rate_following,
        "active_payload_goodput_bps": active_payload_bps,
        "active_body_goodput_bps": active_body_bps,
        "tail_unique": r["late_unique"], "duplicates": r["duplicate"],
    }


def choose_best(stages: list[dict]) -> dict | None:
    passing = [s for s in stages if s.get("sustainable") and s.get("phase") != "warmup"]
    return max(passing, key=lambda s: (s["target_kbps"], s.get("active_body_goodput_bps") or -math.inf),
               default=None)
