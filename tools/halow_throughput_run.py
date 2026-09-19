#!/usr/bin/env python3
"""Bounded serial-controlled HaLow UDP throughput search; no PHY-rate claim."""

from __future__ import annotations

import argparse
import ipaddress
import json
import math
import time
from pathlib import Path

import halow_throughput as throughput
import halow_validation as link_validation
from halow_validation_run import (EXPECTED, EventJournal, Runner, emit, git_state,
                                  open_port, sha256, verify_inventory)

DEFAULT_RATES = (128, 256, 512, 1000, 2000, 4000, 8000)


def command(port, payload: str, events: list[dict], clock, role: str) -> None:
    emit(events, clock, "host_command", role=role, command=payload)
    wire = ("RW_TPUT_CMD " + payload + "\n").encode("ascii")
    written = port.write(wire)
    if written is not None and written != len(wire):
        raise OSError(f"{role}: short serial command write {written}/{len(wire)}")


class ThroughputRunner(Runner):
    def __init__(self, ports, raw_files, *, direction: str, startup_timeout: float,
                 max_seconds: float, warmup_seconds: int, stage_seconds: int,
                 repeat_seconds: int, rates: tuple[int, ...], smoke: bool = False,
                 initial_reset=None,
                 reopen=None, events=None, clock=time.monotonic, sleep=time.sleep):
        super().__init__(ports, raw_files, seconds=max_seconds,
                         ap_ready_timeout=startup_timeout, initial_reset=initial_reset,
                         reopen=reopen, events=events, clock=clock, sleep=sleep)
        self.direction = direction
        self.startup_timeout = startup_timeout
        self.max_seconds = max_seconds
        self.warmup_seconds = warmup_seconds
        self.stage_seconds = stage_seconds
        self.repeat_seconds = repeat_seconds
        self.rates = rates
        self.smoke = smoke
        self.sender, self.receiver = (("sta", "ap") if direction == "sta_to_ap" else ("ap", "sta"))
        self.stage_number = 0
        self.stages: list[dict] = []
        self.ips: dict[str, str] = {}
        self.managed_start = None
        self.stop_started_at = None
        self.deadline = None

    def _line(self, role: str, line: bytes):
        super()._line(role, line)
        for parsed in throughput.parse_lines(line.decode("utf-8", errors="replace")):
            emit(self.events, self.clock, "firmware", role=role, **parsed)
            print(f"[{role}] {parsed['marker']} {parsed['fields']}", flush=True)

    def _rows(self, role=None, marker=None, stage=None):
        return [e for e in self.events if e.get("kind") == "firmware" and
                (role is None or e.get("role") == role) and
                (marker is None or e.get("marker") == marker) and
                (stage is None or throughput.count(e.get("fields", {}), "stage") == stage)]

    def _fatal(self):
        if self.managed_start is not None and any(
               e.get("kind") == "panic" and
               self.managed_start <= e.get("monotonic_s", -1)
               for e in self.events):
            raise RuntimeError("serial panic/watchdog marker")
        if self.managed_start is not None and any(
            e.get("kind") == "firmware" and e.get("marker") == "RW_LINK_BOOT" and
            self.managed_start < e.get("monotonic_s", -1)
            for e in self.events):
            raise RuntimeError("unexpected device boot during throughput session")
        if self.managed_start is not None and any(
            e.get("kind") == "firmware" and e.get("role") == "sta" and
            e.get("marker") == "RW_LINK_STA_STATE" and
            e.get("fields", {}).get("value") in {"0", "1"} and
            self.managed_start < e.get("monotonic_s", -1) < (self.stop_started_at or float("inf"))
            for e in self.events):
            raise RuntimeError("STA disconnected during throughput session")
        if self.managed_start is not None and any(e.get("kind") == "firmware" and e.get("marker") in {
                "RW_TPUT_SESSION_ABORT", "RW_TPUT_STAGE_ABORT", "RW_TPUT_SOCKET_ERROR",
                "RW_TPUT_CMD_REJECT"} and
               self.managed_start <= e.get("monotonic_s", -1) < (self.stop_started_at or float("inf"))
               for e in self.events):
            raise RuntimeError("throughput firmware reported stage/session failure")

    def _wait(self, role: str, marker: str, *, stage=None, timeout: float) -> dict:
        start = self.clock()
        while self.clock() < min(start + timeout, self.deadline or float("inf")):
            self.poll()
            self._fatal()
            found = [e for e in self._rows(role, marker, stage)
                     if e.get("monotonic_s", -1) >= start]
            if found:
                return found[-1]
            self.sleep(.02)
        raise TimeoutError(f"{role}: no {marker} stage={stage} within {timeout}s")

    def _send(self, role: str, payload: str):
        try:
            command(self.ports[role], payload, self.events, self.clock, role)
        except Exception as exc:
            emit(self.events, self.clock, "error", message=f"{role}: command write failed: {exc}")
            raise

    def _startup(self):
        self.deadline = self.clock() + self.max_seconds
        if self.initial_reset:
            for port in self.ports.values():
                reset = getattr(port, "reset_input_buffer", None)
                if callable(reset):
                    reset()
            emit(self.events, self.clock, "host_reset", role="ap", action="serial_reset")
            self.initial_reset(self.ports["ap"])
        self._wait("ap", "RW_LINK_AP_READY", timeout=self.startup_timeout)
        if self.initial_reset:
            emit(self.events, self.clock, "host_reset", role="sta", action="serial_reset")
            self.initial_reset(self.ports["sta"])
        emit(self.events, self.clock, "host", action="sta_started_after_ap_ready")
        # AP READY can predate the STA reset. Keep that marker but require each
        # role's own throughput readiness in the managed session.
        ready = {}
        reset_at = {role: next((e.get("monotonic_s", -1) for e in reversed(self.events)
                                if e.get("kind") == "host_reset" and e.get("role") == role), -1)
                    for role in ("ap", "sta")}
        for role in ("ap", "sta"):
            rows = [e for e in self._rows(role, "RW_TPUT_READY")
                    if e.get("monotonic_s", -1) >= reset_at[role]]
            if not rows:
                ready[role] = self._wait(role, "RW_TPUT_READY", timeout=self.startup_timeout)
            else:
                ready[role] = rows[-1]
            ip = ready[role].get("fields", {}).get("ip")
            try:
                self.ips[role] = str(ipaddress.IPv4Address(ip))
            except ipaddress.AddressValueError as exc:
                raise RuntimeError(f"{role}: no valid IPv4 in RW_TPUT_READY") from exc
        if self.ips["ap"] == self.ips["sta"]:
            raise RuntimeError("AP and STA reported the same IPv4")
        leases = [e for e in self._rows("sta", "RW_LINK_DHCP_LEASE")
                  if e.get("fields", {}).get("ip") == self.ips["sta"] and
                  e.get("monotonic_s", -1) >= reset_at["sta"]]
        if not leases:
            raise RuntimeError("STA actual IPv4 lacks DHCP lease evidence")
        for role in ("ap", "sta"):
            sessions = [e for e in self._rows(role, "RW_TPUT_SESSION_START")
                        if e.get("monotonic_s", -1) >= reset_at[role]]
            if len(sessions) != 1 or sessions[0].get("fields", {}).get("ip") != self.ips[role]:
                raise RuntimeError(f"{role}: missing or mismatched managed throughput session")
        self.managed_start = self.clock()

    def measure(self, rate_kbps: int, seconds: int, phase: str) -> dict:
        self.stage_number += 1
        stage = self.stage_number
        if self.clock() + seconds + 18 >= self.deadline:
            raise TimeoutError("host session deadline leaves insufficient stage time")
        emit(self.events, self.clock, "host_stage", stage=stage, phase=phase,
             rate_kbps=rate_kbps, duration_s=seconds, sender=self.sender,
             receiver=self.receiver)
        self._send(self.receiver, f"ARM stage={stage} peer={self.ips[self.sender]} "
                   f"duration_s={seconds} packet_bytes=1200")
        self._wait(self.receiver, "RW_TPUT_RX_READY", stage=stage, timeout=8)
        self._send(self.sender, f"SEND stage={stage} peer={self.ips[self.receiver]} "
                   f"duration_s={seconds} rate_kbps={rate_kbps} packet_bytes=1200")
        self._wait(self.sender, "RW_TPUT_TX_SUMMARY", stage=stage, timeout=seconds + 12)
        tx = self._rows(self.sender, "RW_TPUT_TX_SUMMARY", stage)[-1].get("fields", {})
        sent = throughput.count(tx, "sent")
        if sent is None:
            raise RuntimeError(f"stage {stage}: no successful-send count for DRAIN")
        self._send(self.receiver, f"DRAIN stage={stage} sent={sent}")
        self._wait(self.receiver, "RW_TPUT_RX_SUMMARY", stage=stage, timeout=9)
        result = throughput.assess_stage(self.events, stage=stage, sender=self.sender,
                                         receiver=self.receiver, rate_kbps=rate_kbps,
                                         requested_s=seconds, phase=phase,
                                         sender_ip=self.ips[self.sender],
                                         receiver_ip=self.ips[self.receiver])
        self.stages.append(result)
        emit(self.events, self.clock, "stage_result", stage=stage, phase=phase,
             valid=result["valid"], sustainable=result["sustainable"],
             active_body_goodput_bps=result["active_body_goodput_bps"])
        if not result["valid"]:
            raise RuntimeError(f"stage {stage}: invalid evidence: {'; '.join(result['errors'])}")
        return result

    def run_search(self):
        self._startup()
        self.measure(self.rates[0], self.warmup_seconds, "warmup")
        passing = []
        failure_rate = None
        for rate in self.rates:
            result = self.measure(rate, self.stage_seconds, "ramp")
            if result["sustainable"]:
                passing.append(result)
            else:
                failure_rate = rate
                break
        if not passing:
            for rate in (64, 32):
                result = self.measure(rate, self.stage_seconds, "lower_probe")
                if result["sustainable"]:
                    passing.append(result)
                    break
        if passing and failure_rate:
            lower, upper = passing[-1]["target_kbps"], failure_rate
            for _ in range(2):
                midpoint = (lower + upper) // 2
                if midpoint <= lower or midpoint >= upper:
                    break
                result = self.measure(midpoint, self.stage_seconds, "refine")
                if result["sustainable"]:
                    passing.append(result)
                    lower = midpoint
                else:
                    upper = midpoint
        candidates = sorted({r["target_kbps"] for r in passing}, reverse=True)
        confirmed = None
        confirmation_incomplete = False
        for candidate in candidates:
            if self.clock() + 3 * (self.repeat_seconds + 9) >= self.deadline:
                confirmation_incomplete = True
                break
            repeats = [self.measure(candidate, self.repeat_seconds, "confirm") for _ in range(3)]
            if all(r["sustainable"] for r in repeats):
                confirmed = {"target_kbps": candidate, "stages": repeats}
                break
        saturation = None if self.smoke else self.measure(0, self.stage_seconds, "saturation")
        return {"confirmed": confirmed, "saturation": saturation,
                "search_ceiling_reached": failure_rate is None,
                "first_failed_rate_kbps": failure_rate,
                "confirmation_incomplete": confirmation_incomplete}

    def _shutdown_evidence(self, role: str) -> tuple[bool, bool]:
        stops = [e for e in self.events if e.get("kind") == "host_command" and
                 e.get("role") == role and e.get("command") == "STOP"]
        if not stops:
            return False, False
        since = stops[-1].get("monotonic_s", -1)
        rows = [e for e in self._rows(role) if e.get("monotonic_s", -1) >= since]
        markers = {e.get("marker") for e in rows}
        if not {"RW_TPUT_CMD_ACK", "RW_TPUT_SESSION_END", "RW_LINK_RADIO_SHUTDOWN",
                "RW_LINK_RADIO_RESULT", "RW_LINK_DONE"} <= markers:
            return False, False
        if not any(e.get("marker") == "RW_TPUT_CMD_ACK" and
                   e.get("fields", {}).get("command") == "STOP" for e in rows):
            return False, False
        session = [e.get("fields", {}) for e in rows if e.get("marker") == "RW_TPUT_SESSION_END"][-1]
        result = [e.get("fields", {}).get("value") for e in rows if e.get("marker") == "RW_LINK_RADIO_RESULT"][-1]
        return True, (throughput.truth(session, "stopped") is True and
                      throughput.truth(session, "all_stages_ok") is True and result == "PASS")

    def _complete(self, role: str) -> bool:
        return all(self._shutdown_evidence(role))

    def stop_and_drain(self):
        self.stop_started_at = self.clock()
        for role in ("sta", "ap"):
            if role not in self.ports or self._shutdown_evidence(role)[0]:
                continue
            try:
                self._send(role, "STOP")
            except Exception:
                pass
        deadline = self.clock() + 25
        while self.clock() < deadline and not all(self._shutdown_evidence(r)[0] for r in self.ports):
            try:
                self.poll()
            except Exception as exc:
                emit(self.events, self.clock, "error", message=f"cleanup capture failed: {exc}")
                break
            self.sleep(.05)
        for role in self.ports:
            present, successful = self._shutdown_evidence(role)
            if not present:
                emit(self.events, self.clock, "error",
                     message=f"{role}: missing STOP ACK/session end/radio shutdown/DONE")
            elif not successful:
                emit(self.events, self.clock, "error",
                     message=f"{role}: session or radio result FAIL after complete shutdown")


def markdown(report: dict) -> str:
    lines = [f"# HaLow UDP throughput: {report['status']}", "",
             f"Direction: {report['direction']}; width: {report['bandwidth_mhz']} MHz.", "",
             "| Phase | Stage | Target kbit/s | Actual TX bit/s | Active body goodput bit/s | Active/final delivery | Sustainable |",
             "|---|---:|---:|---:|---:|---:|---|"]
    for row in report["stages"]:
        fmt = lambda value: "NA" if value is None else f"{value:,.0f}"
        ratio = lambda value: "NA" if value is None else f"{100*value:.2f}%"
        lines.append(f"| {row['phase']} | {row['stage']} | {row['target_kbps']} | "
                     f"{fmt(row['actual_tx_bps'])} | {fmt(row['active_body_goodput_bps'])} | "
                     f"{ratio(row['active_delivery'])}/{ratio(row['final_delivery'])} | "
                     f"{'yes' if row['sustainable'] else 'no'} |")
    confirmed = report.get("confirmed")
    lines += ["", f"Confirmed highest tested rate: {confirmed['target_kbps']} kbit/s" if confirmed else
              "No 3×60 s sustainable rate was confirmed.",
              "This is a bounded path measurement, not a PHY maximum or long-term capacity claim."]
    if report.get("search_ceiling_reached"):
        lines.append("The paced search reached its configured rate ceiling without a failing boundary.")
    if report.get("confirmation_incomplete"):
        lines.append("The session deadline prevented testing every lower-rate fallback.")
    if report["errors"]:
        lines += ["", "Errors:"] + [f"- {error}" for error in report["errors"]]
    return "\n".join(lines) + "\n"


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--direction", choices=("sta_to_ap", "ap_to_sta"), required=True)
    p.add_argument("--bandwidth-mhz", type=int, choices=(1, 2), required=True)
    p.add_argument("--sta", choices=("COM5", "COM6"), required=True)
    p.add_argument("--ap", choices=("COM4",), default="COM4")
    p.add_argument("--output-dir", type=Path, required=True)
    p.add_argument("--public-log", type=Path)
    p.add_argument("--ap-bin", type=Path, required=True)
    p.add_argument("--sta-bin", type=Path, required=True)
    p.add_argument("--fw-source-commit", required=True)
    p.add_argument("--warmup-seconds", type=int, default=5)
    p.add_argument("--stage-seconds", type=int, default=30)
    p.add_argument("--repeat-seconds", type=int, default=60)
    p.add_argument("--rates-kbps", type=int, nargs="+", default=DEFAULT_RATES)
    p.add_argument("--smoke", action="store_true",
                   help="skip the unpaced stage and label outcome SMOKE_PASS")
    p.add_argument("--startup-timeout", type=float, default=90)
    p.add_argument("--max-seconds", type=float, default=1500)
    p.add_argument("--baud", type=int, default=115200)
    args = p.parse_args()
    if not all(path.is_file() for path in (args.ap_bin, args.sta_bin)):
        p.error("both flashed firmware binaries must exist for SHA256 manifest")
    if any(n < 5 for n in (args.warmup_seconds, args.stage_seconds, args.repeat_seconds)):
        p.error("stage durations must be at least 5 s per firmware command contract")
    if not args.rates_kbps or any(n <= 0 for n in args.rates_kbps) or sorted(set(args.rates_kbps)) != list(args.rates_kbps):
        p.error("--rates-kbps must be unique positive values in ascending order")
    if not math.isfinite(args.max_seconds) or not 0 < args.max_seconds <= 1700:
        p.error("--max-seconds must be bounded within the 1800s firmware session")
    if args.output_dir.exists() and any(args.output_dir.iterdir()):
        p.error("--output-dir must be empty; evidence is never overwritten")
    if ".private" not in args.output_dir.resolve().parts:
        p.error("--output-dir must be beneath .private (raw logs may contain secrets)")
    args.output_dir.mkdir(parents=True, exist_ok=True)
    events = EventJournal(args.output_dir / "events.jsonl")
    ports, streams = {}, {}
    runner = None
    search = {}
    try:
        import serial
        import serial.tools.list_ports
        from esp_idf_monitor.base.reset import Reset
        devices = {"ap": args.ap, "sta": args.sta}
        verify_inventory(serial, *devices.values())
        for role, name in devices.items():
            ports[role] = open_port(serial, name, args.baud)
            streams[role] = (args.output_dir / f"{role}-raw.log").open("wb")
        runner = ThroughputRunner(ports, streams, direction=args.direction,
                                  startup_timeout=args.startup_timeout,
                                  max_seconds=args.max_seconds,
                                  warmup_seconds=args.warmup_seconds,
                                  stage_seconds=args.stage_seconds,
                                  repeat_seconds=args.repeat_seconds,
                                  rates=tuple(args.rates_kbps), smoke=args.smoke,
                                  initial_reset=lambda port: Reset(port, "esp32s3").hard(),
                                  reopen=lambda role: (verify_inventory(serial, devices[role]),
                                                       open_port(serial, devices[role], args.baud))[1],
                                  events=events)
        for role, name in devices.items():
            emit(events, time.monotonic, "host_inventory", role=role, port=name, mac=EXPECTED[name])
        search = runner.run_search()
    except (Exception, KeyboardInterrupt) as exc:
        emit(events, time.monotonic, "error",
             message=f"capture interrupted or failed: {type(exc).__name__}: {exc}")
        if runner is not None:
            for role in ("sta", "ap"):
                if runner.stage_number:
                    try:
                        runner._send(role, f"ABORT stage={runner.stage_number}")
                    except Exception:
                        pass
    finally:
        if runner is not None:
            try:
                runner.stop_and_drain()
            except (Exception, KeyboardInterrupt) as exc:
                emit(events, time.monotonic, "error", message=f"cleanup failed: {exc}")
        for stream in streams.values():
            try:
                stream.close()
            except OSError as exc:
                emit(events, time.monotonic, "error", message=f"raw close failed: {exc}")
        for port in ports.values():
            try:
                port.close()
            except Exception as exc:
                emit(events, time.monotonic, "error", message=f"serial close failed: {exc}")
        events.finalize()
    errors = [e["message"] for e in events if e.get("kind") == "error"]
    if runner is not None:
        try:
            runner._fatal()
        except RuntimeError as exc:
            errors.append(str(exc))
        for role in ("ap", "sta"):
            present, successful = runner._shutdown_evidence(role)
            if not present:
                errors.append(f"{role}: incomplete final shutdown evidence")
            elif not successful:
                errors.append(f"{role}: complete shutdown but unsuccessful session/radio result")
    report = {
        "status": ("SMOKE_PASS" if args.smoke else "PASS") if not errors and search.get("confirmed") else "FAIL",
        "mode": "smoke" if args.smoke else "full",
        "direction": args.direction, "bandwidth_mhz": args.bandwidth_mhz,
        "stages": runner.stages if runner is not None else [],
        "confirmed": search.get("confirmed"),
        "saturation": search.get("saturation"),
        "search_ceiling_reached": search.get("search_ceiling_reached"),
        "confirmation_incomplete": search.get("confirmation_incomplete"),
        "first_failed_rate_kbps": search.get("first_failed_rate_kbps"),
        "errors": errors,
        "manifest": {"firmware_source_commit": args.fw_source_commit,
                     "ap": {"port": args.ap, "mac": EXPECTED[args.ap], "binary_sha256": sha256(args.ap_bin)},
                     "sta": {"port": args.sta, "mac": EXPECTED[args.sta], "binary_sha256": sha256(args.sta_bin)},
                     "host_git": git_state(), "raw_dir": str(args.output_dir.resolve())},
    }
    (args.output_dir / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    (args.output_dir / "report.md").write_text(markdown(report), encoding="utf-8")
    if args.public_log:
        args.public_log.parent.mkdir(parents=True, exist_ok=True)
        args.public_log.write_text("".join(
            f"{e['iso_time']} {e['role']} {e['marker']} " +
            " ".join(f"{k}={v}" for k, v in e["fields"].items()) + "\n"
            for e in events if e.get("kind") == "firmware"), encoding="utf-8")
    print(f"{report['status']}: {args.output_dir / 'report.md'}", flush=True)
    return 0 if report["status"] in {"PASS", "SMOKE_PASS"} else 1


if __name__ == "__main__":
    raise SystemExit(main())
