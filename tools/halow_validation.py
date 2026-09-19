#!/usr/bin/env python3
"""Analyze bounded HaLow link telemetry without opening serial ports."""

from __future__ import annotations

import argparse
import json
import math
import re
from pathlib import Path

MARKER = re.compile(r"RW_LINK_[A-Z_]+")
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
    "RW_LINK_DHCP_CLIENT", "RW_LINK_DHCP_SERVER", "RW_LINK_DHCP_LEASE",
    "RW_LINK_UDP_BIND", "RW_LINK_AP_PEER",
    "RW_LINK_IRQ_QUIESCED", "RW_LINK_RESTART_READY", "RW_LINK_RESTART_ABORT",
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
    "acquire_ms", "t_ms", "gw", "mask", "mac", "start_err", "err",
    "noise_dbm", "op_bw_mhz", "scan_snr_db", "scan_snr_status",
    "gpio", "radio_stopped",
}


def parse_lines(line: str) -> list[dict]:
    """Extract all known telemetry, including markers joined to driver text."""
    matches = list(MARKER.finditer(line))
    output = []
    for index, match in enumerate(matches):
        marker = match.group(0)
        if marker not in ALLOWED:
            continue
        end = matches[index + 1].start() if index + 1 < len(matches) else len(line)
        segment = line[match.end():end]
        fields = {k: v for k, v in FIELD.findall(segment) if k in SAFE_FIELDS}
        value = re.match(r"=([^\s;]+)", segment)
        if value is not None:
            fields["value"] = value.group(1)
        output.append({"marker": marker, "fields": fields})
    return output


def parse_line(line: str) -> dict | None:
    """Return the first known telemetry marker for simple callers."""
    events = parse_lines(line)
    return events[0] if events else None


def number(fields: dict, key: str) -> int | float | None:
    try:
        n = float(fields[key])
    except (KeyError, TypeError, ValueError):
        return None
    return n if math.isfinite(n) and n >= 0 else None


def station_roles(expected_roles) -> tuple[str, ...]:
    return tuple(role for role in expected_roles if role != "ap")


def managed_events(events: list[dict]) -> list[dict]:
    """Exclude telemetry seen before each role's initial host serial reset.

    A freshly flashed STA may boot on its own while the host is starting AP.
    Those bytes stay in the raw/event files, but cannot enter this run's gates.
    """
    gates = {}
    for event in events:
        role = event.get("role")
        if role not in {"ap", "sta", "sta2"}:
            continue
        if event.get("kind") == "host_reset" and event.get("action") == "serial_reset":
            gates.setdefault(role, event.get("monotonic_s", 0))
    return [event for event in events if not (
        event.get("kind") == "firmware" and event.get("role") in gates and
        event.get("monotonic_s", -1) < gates[event["role"]])]


def analyze(events: list[dict], expected_roles=None) -> dict:
    raw_event_count = len(events)
    events = managed_events(events)
    if expected_roles is None:
        inventory_roles = {e.get("role") for e in events if e.get("kind") == "host_inventory"}
        expected_roles = ("ap", "sta", "sta2") if "sta2" in inventory_roles else ("ap", "sta")
    errors: list[str] = []
    for event in events:
        if event.get("kind") == "firmware" and event.get("marker") == "RW_LINK_RESTART_ABORT":
            errors.append(f"{event.get('role', 'device')}: firmware aborted software restart")
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
        if role != "ap":
            if sent is None or received is None or lost is None:
                errors.append(f"{role}: summary lacks valid sent/received/lost counts")
            elif sent <= 0 or received <= 0 or received > sent or lost != sent - received:
                errors.append(f"{role}: summary counters are inconsistent or have no valid echoes")
        previous = None
        counter_keys = ("sent", "received", "lost") if role != "ap" else ("echo_id1", "echo_id2")
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
        if role != "ap" and received is not None and len(final_sequences) != received:
            errors.append(f"{role}: final run logged valid echo count differs from summary received")
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
        if role != "ap":
            scan_epoch = boots[-1].get("monotonic_s", -1) if boots else -1
            scans = [e.get("fields", {}) for e in row if e.get("marker") == "RW_LINK_SCAN_TARGET" and
                     e.get("monotonic_s", -1) >= scan_epoch]
            roles[role]["scan_target"] = scans[-1] if scans else None
            configured_ip = (starts[-1].get("fields", {}) if starts else {}).get("ip")
            if configured_ip and configured_ip.upper() == "DHCP":
                roles[role]["ip_mode"] = "dhcp"
                epoch_start = boots[-1].get("monotonic_s", -1) if boots else None
                roles[role]["dhcp_epoch_boot_monotonic_s"] = epoch_start
                if epoch_start is None:
                    errors.append(f"{role}: DHCP boot marker missing")
                leases = [e.get("fields", {}) for e in row if e.get("marker") == "RW_LINK_DHCP_LEASE" and
                          epoch_start is not None and e.get("monotonic_s", -1) >= epoch_start]
                binds = [e.get("fields", {}) for e in row if e.get("marker") == "RW_LINK_UDP_BIND" and
                         epoch_start is not None and e.get("monotonic_s", -1) >= max(epoch_start, starts[-1].get("monotonic_s", -1))]
                effective_ip = (binds[-1].get("ip") if binds else None) or (leases[-1].get("ip") if leases else None)
                roles[role]["dhcp_lease"] = leases[-1] if leases else None
                roles[role]["udp_bind"] = binds[-1] if binds else None
                if not leases or not binds or not effective_ip or effective_ip.upper() == "DHCP":
                    errors.append(f"{role}: DHCP run lacks lease or UDP bind address")
                elif leases[-1].get("ip") != binds[-1].get("ip"):
                    errors.append(f"{role}: DHCP lease and UDP bind addresses differ")
                if not any(e.get("marker") == "RW_LINK_DHCP_CLIENT" and
                           e.get("fields", {}).get("value") == "ENABLED" and
                           epoch_start is not None and e.get("monotonic_s", -1) >= epoch_start for e in row):
                    errors.append(f"{role}: DHCP client start was not recorded")
            else:
                roles[role]["ip_mode"] = "static"
                effective_ip = configured_ip
            roles[role]["effective_ip"] = effective_ip
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
            if e.get("marker") == "RW_LINK_DHCP_SERVER" and e.get("fields", {}).get("value") in {"FAIL", "STOP_FAIL"}:
                errors.append(f"{role}: DHCP server failed")
    faults = [e for e in events if e.get("kind") == "fault"]
    ap_ready = [e for e in events if e.get("role") == "ap" and e.get("marker") == "RW_LINK_AP_READY"]
    for station in station_roles(expected_roles):
        sta_start = [e for e in events if e.get("role") == station and e.get("marker") == "RW_LINK_RUN_START"]
        if not ap_ready or not sta_start or ap_ready[0].get("monotonic_s", 0) > sta_start[0].get("monotonic_s", 0):
            errors.append(f"{station}: run did not follow a recorded AP_READY gate")
    inventory = {e.get("role"): e for e in events if e.get("kind") == "host_inventory"}
    for station in station_roles(expected_roles):
        port = inventory.get(station, {}).get("port")
        expected_id = {"COM5": "1", "COM6": "2"}.get(port)
        start = roles.get(station, {}).get("run_start") or {}
        if expected_id and start.get("id") != expected_id:
            errors.append(f"{station}: firmware ID does not match {port}")
    if any(roles.get(station, {}).get("ip_mode") == "dhcp" for station in station_roles(expected_roles)):
        ap_boots = [e for e in events if e.get("role") == "ap" and e.get("marker") == "RW_LINK_BOOT"]
        ap_epoch = ap_boots[-1].get("monotonic_s", -1) if ap_boots else None
        if ap_epoch is None:
            errors.append("DHCP AP boot marker missing")
        if not any(e.get("role") == "ap" and e.get("marker") == "RW_LINK_DHCP_SERVER" and
                   e.get("fields", {}).get("value") == "STARTED" and ap_epoch is not None and
                   e.get("monotonic_s", -1) >= ap_epoch for e in events):
            errors.append("DHCP server start was not recorded")
        peer_rows = [e.get("fields", {}) for e in events if e.get("role") == "ap" and
                     e.get("marker") == "RW_LINK_AP_PEER" and ap_epoch is not None and
                     e.get("monotonic_s", -1) >= ap_epoch]
        ap_leases = [e.get("fields", {}) for e in events if e.get("role") == "ap" and
                     e.get("marker") == "RW_LINK_DHCP_LEASE" and ap_epoch is not None and
                     e.get("monotonic_s", -1) >= ap_epoch]
        for station in station_roles(expected_roles):
            if roles.get(station, {}).get("ip_mode") != "dhcp":
                continue
            peer_id = (roles.get(station, {}).get("run_start") or {}).get("id")
            matching = [row for row in peer_rows if row.get("id") == peer_id and
                        row.get("ip") == roles[station]["effective_ip"] and row.get("mac")]
            if not matching:
                errors.append(f"{station}: AP peer does not match DHCP address")
            elif not any(lease.get("ip") == matching[-1].get("ip") and
                         lease.get("mac", "").lower() == matching[-1].get("mac", "").lower()
                         for lease in ap_leases):
                errors.append(f"{station}: AP peer lacks matching DHCP lease")
    if "sta2" in expected_roles:
        ap_start = roles.get("ap", {}).get("run_start") or {}
        if number(ap_start, "max_stas") is None or number(ap_start, "max_stas") < 2:
            errors.append("three-node: AP run did not advertise capacity for two STAs")
        nonce1 = (roles.get("sta", {}).get("run_start") or {}).get("run_id")
        nonce2 = (roles.get("sta2", {}).get("run_start") or {}).get("run_id")
        if not nonce1 or not nonce2 or nonce1 == nonce2:
            errors.append("three-node: STA run IDs are missing or identical")
        ip1 = roles.get("sta", {}).get("effective_ip")
        ip2 = roles.get("sta2", {}).get("effective_ip")
        if not ip1 or not ip2 or ip1 == ip2:
            errors.append("three-node: STA IPs are missing or identical")
    ap_summary = roles.get("ap", {}).get("summary") or {}
    ap_starts = [e for e in events if e.get("role") == "ap" and e.get("marker") == "RW_LINK_RUN_START"]
    ap_summaries = [e for e in events if e.get("role") == "ap" and e.get("marker") == "RW_LINK_SUMMARY"]
    ap_scope_start = ap_starts[-1].get("monotonic_s", -1) if ap_starts else float("inf")
    ap_scope_end = ap_summaries[-1].get("monotonic_s", -1) if ap_summaries else -1
    peer_matches = {}
    for station in station_roles(expected_roles):
        peer_id = (roles.get(station, {}).get("run_start") or {}).get("id")
        if peer_id not in {"1", "2"}:
            continue  # Legacy offline captures did not record the station ID.
        echoed = number(ap_summary, f"echo_id{peer_id}")
        matched = sum(e.get("kind") == "firmware" and e.get("role") == station and
                      e.get("marker") == "RW_LINK_ECHO_MATCH" and
                      ap_scope_start <= e.get("monotonic_s", -1) <= ap_scope_end for e in events)
        peer_matches[station] = matched
        if echoed is None or echoed < matched:
            label = "three-node" if "sta2" in expected_roles else station
            errors.append(f"{label}: AP echo_id{peer_id} does not cover {station} exact replies")
    roles["ap"]["peer_exact_matches_in_final_ap_run"] = peer_matches
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
            if boot is not None:
                boot_time = boot.get("monotonic_s", -1)
                epoch_events = [e for e in events if e.get("kind") == "firmware" and
                                boot_time <= e.get("monotonic_s", -1) < next_fault_time]
                if role != "ap" and roles.get(role, {}).get("ip_mode") == "dhcp":
                    own = [e for e in epoch_events if e.get("role") == role]
                    leases = [e.get("fields", {}) for e in own if e.get("marker") == "RW_LINK_DHCP_LEASE"]
                    binds = [e.get("fields", {}) for e in own if e.get("marker") == "RW_LINK_UDP_BIND"]
                    client = any(e.get("marker") == "RW_LINK_DHCP_CLIENT" and
                                 e.get("fields", {}).get("value") == "ENABLED" for e in own)
                    if not client or not leases or not binds or leases[-1].get("ip") != binds[-1].get("ip"):
                        errors.append(f"{role}: DHCP lease/bind not renewed after software reset")
                if role == "ap" and any(roles.get(station, {}).get("ip_mode") == "dhcp"
                                        for station in station_roles(expected_roles)):
                    ap_epoch_events = [e for e in epoch_events if e.get("role") == "ap"]
                    if not any(e.get("marker") == "RW_LINK_DHCP_SERVER" and
                               e.get("fields", {}).get("value") == "STARTED" for e in ap_epoch_events):
                        errors.append("ap: DHCP server did not restart after software reset")
                    ap_leases = [e.get("fields", {}) for e in ap_epoch_events if e.get("marker") == "RW_LINK_DHCP_LEASE"]
                    for station in station_roles(expected_roles):
                        peer_id = (roles.get(station, {}).get("run_start") or {}).get("id")
                        peers = [e.get("fields", {}) for e in ap_epoch_events if e.get("marker") == "RW_LINK_AP_PEER" and
                                 e.get("fields", {}).get("id") == peer_id]
                        if not peers or not any(lease.get("ip") == peers[-1].get("ip") and
                                                lease.get("mac", "").lower() == peers[-1].get("mac", "").lower()
                                                for lease in ap_leases):
                            errors.append(f"ap: no renewed DHCP lease/peer mapping for {station} after reset")
        elif any(e.get("marker") == "RW_LINK_BOOT" for e in after):
            errors.append(f"{role}: AP service outage unexpectedly rebooted")
        # The STA reports link recovery even when the AP was the fault target.
        if role == "ap":
            for station in station_roles(expected_roles):
                recovery_after = [e for e in events if e.get("role") == station and
                                  e.get("kind") == "firmware" and
                                  fault.get("monotonic_s", float("inf")) < e.get("monotonic_s", -1) < next_fault_time]
                if not any(e.get("marker") == "RW_LINK_RECOVERY" for e in recovery_after):
                    errors.append(f"{station}: no recovery marker after AP fault")
    runner_errors = [e.get("message", "runner failure") for e in events if e.get("kind") == "error"]
    errors.extend(runner_errors)
    errors.extend(f"{e.get('role', '?')}: raw serial panic/watchdog marker {e.get('code', '?')}"
                  for e in events if e.get("kind") == "panic")
    return {"status": "PASS" if not errors else "FAIL", "errors": errors,
            "roles": roles, "faults": faults, "event_count": len(events),
            "ignored_prestart_events": raw_event_count - len(events), "cold_boot_counted": 0}


def longest_streak(echoes: list[dict]) -> int:
    best = current = 0
    previous = None
    for event in echoes:
        seq = number(event.get("fields", {}), "seq")
        current = current + 1 if seq is not None and (previous is None or seq == previous + 1) else 1
        best = max(best, current)
        previous = seq
    return best


def apply_acceptance(report: dict, events: list[dict], *, min_sent=200,
                     min_delivery=0.99, min_duration_s=120,
                     recovery_limit_s=60, consecutive=20) -> dict:
    """Apply the P0-A finite-run gates to an analyzed capture."""
    events = managed_events(events)
    errors = report["errors"]
    stations = tuple(role for role in report["roles"] if role != "ap")
    faults = report["faults"]
    if not faults:
        report["post_warmup_by_role"] = {}
        warmup_times = {}
        end_times = {}
        for station in stations:
            summary = report["roles"].get(station, {}).get("summary") or {}
            sent, received = number(summary, "sent"), number(summary, "received")
            samples = [e for e in events if e.get("role") == station and e.get("marker") == "RW_LINK_SAMPLE"]
            warmup = next((e for e in samples if number(e.get("fields", {}), "elapsed_ms") is not None
                           and number(e.get("fields", {}), "elapsed_ms") >= 10000), None)
            ends = [e for e in events if e.get("role") == station and e.get("marker") == "RW_LINK_SUMMARY"]
            if warmup is None:
                errors.append(f"{station} baseline: missing 10s warmup sample")
                continue
            warmup_times[station] = warmup.get("monotonic_s", 0)
            if ends:
                end_times[station] = ends[-1].get("monotonic_s", 0)
            warmup_sent = number(warmup.get("fields", {}), "sent")
            warmup_received = number(warmup.get("fields", {}), "received")
            post_sent = sent - warmup_sent if sent is not None and warmup_sent is not None else None
            post_received = received - warmup_received if received is not None and warmup_received is not None else None
            details = {"sent": post_sent, "received": post_received,
                       "warmup_elapsed_ms": number(warmup.get("fields", {}), "elapsed_ms"),
                       "duration_s": ends[-1].get("monotonic_s", 0) - warmup.get("monotonic_s", 0) if ends else None}
            report["post_warmup_by_role"][station] = details
            if len(stations) == 1:
                report["post_warmup"] = details
            if post_sent is None or post_sent < min_sent:
                errors.append(f"{station} baseline: fewer than {min_sent} successfully sent probes after warmup")
            if post_sent is None or post_received is None or post_sent <= 0 or post_received / post_sent < min_delivery:
                errors.append(f"{station} baseline: post-warmup valid echo delivery below {min_delivery:.1%}")
            if not ends or ends[-1].get("monotonic_s", 0) - warmup.get("monotonic_s", 0) < min_duration_s:
                errors.append(f"{station} baseline: post-warmup observation shorter than {min_duration_s}s")
        if len(stations) == 2:
            overlap = min(end_times.values()) - max(warmup_times.values()) if len(end_times) == 2 and len(warmup_times) == 2 else None
            report["simultaneous_post_warmup_s"] = overlap
            if overlap is None or overlap < min_duration_s:
                errors.append(f"three-node: simultaneous post-warmup interval shorter than {min_duration_s}s")
            else:
                common_start, common_end = max(warmup_times.values()), min(end_times.values())
                report["simultaneous_exact_echoes"] = {}
                for station in stations:
                    count = sum(e.get("role") == station and e.get("marker") == "RW_LINK_ECHO_MATCH" and
                                common_start < e.get("monotonic_s", -1) < common_end for e in events)
                    report["simultaneous_exact_echoes"][station] = count
                    if count < min_sent:
                        errors.append(f"three-node: {station} has fewer than {min_sent} exact echoes in simultaneous interval")
    else:
        for index, fault in enumerate(faults):
            start = fault.get("monotonic_s", 0)
            end = faults[index + 1].get("monotonic_s", float("inf")) if index + 1 < len(faults) else float("inf")
            role = fault.get("role")
            targets = stations if role == "ap" else (role,)
            fault["recovery_by_role"] = {}
            for station in targets:
                anchor_marker, anchor_role = ("RW_LINK_AP_READY", "ap") if role == "ap" else ("RW_LINK_BOOT", station)
                anchors = [e for e in events if start < e.get("monotonic_s", -1) < end and
                           e.get("role") == anchor_role and e.get("marker") == anchor_marker]
                if not anchors:
                    errors.append(f"fault {index + 1} {station}: missing recovery timing anchor {anchor_marker}")
                    continue
                anchor = anchors[0].get("monotonic_s", 0)
                echoes = [e for e in events if anchor < e.get("monotonic_s", -1) < end and
                          e.get("role") == station and e.get("marker") == "RW_LINK_ECHO_MATCH"]
                if not echoes or echoes[0]["monotonic_s"] - anchor > recovery_limit_s:
                    errors.append(f"fault {index + 1} {station}: valid echo did not resume within {recovery_limit_s}s")
                    continue
                best = longest_streak(echoes)
                details = {"anchor_monotonic_s": anchor,
                           "first_echo_monotonic_s": echoes[0]["monotonic_s"],
                           "recovery_s": echoes[0]["monotonic_s"] - anchor,
                           "longest_exact_echo_streak": best}
                fault["recovery_by_role"][station] = details
                if len(targets) == 1:
                    fault.update(details)
                if best < consecutive:
                    errors.append(f"fault {index + 1} {station}: fewer than {consecutive} consecutive exact echoes")
                if role != "ap" and len(stations) == 2:
                    other = next(s for s in stations if s != station)
                    before = [e for e in events if e.get("role") == other and
                              e.get("marker") == "RW_LINK_ECHO_MATCH" and
                              e.get("monotonic_s", -1) <= start]
                    other_echoes = [e for e in events if e.get("role") == other and
                                    e.get("marker") == "RW_LINK_ECHO_MATCH" and
                                    start < e.get("monotonic_s", -1) < echoes[0]["monotonic_s"]]
                    after = [e for e in events if e.get("role") == other and
                             e.get("marker") == "RW_LINK_ECHO_MATCH" and
                             echoes[0]["monotonic_s"] <= e.get("monotonic_s", -1) < end]
                    if not other_echoes:
                        errors.append(f"fault {index + 1}: {other} had no exact echo while {station} restarted")
                    if not before or not after:
                        errors.append(f"fault {index + 1}: {other} lacks echo evidence across {station} restart")
                    span = ([before[-1]] if before else []) + other_echoes + ([after[0]] if after else [])
                    max_gap = max((b["monotonic_s"] - a["monotonic_s"] for a, b in zip(span, span[1:])), default=float("inf"))
                    if max_gap > 5:
                        errors.append(f"fault {index + 1}: {other} echo gap exceeded 5s during {station} restart")
                    other_failures = [e for e in events if e.get("role") == other and
                                      e.get("marker") in {"RW_LINK_OUTAGE", "RW_LINK_RECOVERY", "RW_LINK_TIMEOUT", "RW_LINK_SEND_FAIL", "RW_LINK_RUN_START"} and
                                      start < e.get("monotonic_s", -1) < echoes[0]["monotonic_s"]]
                    if other_failures:
                        errors.append(f"fault {index + 1}: {other} had an outage while {station} restarted")
                    fault["unaffected_station"] = other
                    fault["unaffected_exact_echoes_during_restart"] = len(other_echoes)
                    fault["unaffected_max_echo_gap_s"] = max_gap
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
    for role in station_roles(report["roles"]):
        role_report = report["roles"].get(role, {})
        sta = role_report.get("summary") or {}
        post = report.get("post_warmup_by_role", {}).get(role)
        lines.extend(["", f"{role} network: {role_report.get('ip_mode', '—')} "
                      f"{role_report.get('effective_ip', '—')}."])
        if post:
            lines.extend(["", f"{role} after 10s warmup: {post.get('sent')} sent, "
                          f"{post.get('received')} exact replies over {post.get('duration_s', '—')}s."])
        if sta:
            scan = role_report.get("scan_target") or {}
            if scan:
                lines.extend(["", f"{role} scan RSSI/noise/SNR: {scan.get('rssi_dbm', '—')}/"
                              f"{scan.get('noise_dbm', '—')}/{scan.get('scan_snr_db', 'NA')} dB; "
                              f"SNR status: {scan.get('scan_snr_status', '—')} (scan-time only)."])
            lines.extend(["", f"{role} RTT p50/p95/max: {sta.get('rtt_p50_us', '—')}/"
                          f"{sta.get('rtt_p95_us', '—')}/{sta.get('rtt_max_us', '—')} µs; "
                          f"offered/useful: {sta.get('offered_bps', '—')}/"
                          f"{sta.get('useful_bps', '—')} bps; RSSI: {sta.get('rssi_dbm', '—')} dBm.",
                          f"{role} send failures/skipped/invalid: {sta.get('send_fail', '—')}/"
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
                         f"recovery={f.get('recovery_by_role', {})}, "
                         f"unaffected station echoes={f.get('unaffected_exact_echoes_during_restart', '—')}, "
                         f"max echo gap={f.get('unaffected_max_echo_gap_s', '—')}s")
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
