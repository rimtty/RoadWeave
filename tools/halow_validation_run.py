#!/usr/bin/env python3
"""Bounded two- or three-port HaLow capture, software faults, and strict report.

Run COM4/AP with COM5 or COM6; optionally run both stations together. It never powers a
device off or credits a serial/firmware reset as a physical cold boot.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

import halow_validation as validation

EXPECTED = {
    "COM4": "44:B1:76:B0:57:20",
    "COM5": "44:B1:76:B0:57:1C",
    "COM6": "44:B1:76:AE:C4:90",
}
WARNING_PATTERNS = {
    "address_base_set_failed": "Address base set failed",
    "unknown_tlv": "Unknown TLV",
}
PANIC_PATTERNS = {
    "guru_meditation": "Guru Meditation Error",
    "abort": "abort() was called",
    "task_watchdog": "Task watchdog got triggered",
    "stack_smashing": "Stack smashing protect failure",
    "brownout": "Brownout detector was triggered",
}


def now_iso() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds")


def emit(events: list[dict], clock, kind: str, **fields) -> dict:
    item = {"kind": kind, "iso_time": now_iso(), "monotonic_s": clock(), **fields}
    events.append(item)
    return item


class EventJournal(list):
    """Flush each event so an interrupted capture retains host fault evidence."""

    def __init__(self, path: Path):
        super().__init__()
        self.path = path
        self.stream = path.open("w", encoding="utf-8", newline="\n")

    def append(self, item: dict) -> None:
        super().append(item)
        self.stream.write(json.dumps(item, ensure_ascii=False) + "\n")
        self.stream.flush()

    def finalize(self) -> None:
        # Fault ACK updates mutate their original event in memory. Rewrite once
        # after capture, atomically, while the flushed journal remains intact.
        self.stream.close()
        temporary = self.path.with_name(self.path.name + ".tmp")
        temporary.write_text("".join(json.dumps(e, ensure_ascii=False) + "\n" for e in self),
                             encoding="utf-8")
        temporary.replace(self.path)


def parse_fault(value: str) -> tuple[str, str, float]:
    try:
        role, action, at = value.split(":", 2)
        seconds = float(at)
    except ValueError as exc:
        raise argparse.ArgumentTypeError("fault format is ROLE:ACTION:SECONDS") from exc
    if role not in {"ap", "sta", "sta2"} or action not in {"software_reset", "ap_off_10s"}:
        raise argparse.ArgumentTypeError("supported faults: ap/sta/sta2:software_reset or ap:ap_off_10s")
    if action == "ap_off_10s" and role != "ap":
        raise argparse.ArgumentTypeError("ap_off_10s is AP-only")
    if not math.isfinite(seconds) or seconds < 0:
        raise argparse.ArgumentTypeError("fault seconds must be finite and nonnegative")
    return role, action, seconds


def verify_device(serial_module, device: str) -> None:
    inventory = {p.device.upper(): (p.serial_number or "").upper() for p in serial_module.tools.list_ports.comports()}
    expected = EXPECTED[device]
    actual = inventory.get(device, "")
    if actual != expected:
        raise RuntimeError(f"{device} identity mismatch: expected {expected}, got {actual or 'missing'}")


def verify_inventory(serial_module, *devices: str) -> None:
    for device in devices:
        verify_device(serial_module, device)


def open_port(serial_module, name: str, baud: int):
    port = serial_module.Serial(port=None, baudrate=baud, timeout=0.1, write_timeout=2.0)
    port.dtr = False
    port.rts = False
    port.port = name
    port.open()
    return port


def sha256(path: Path | None) -> str | None:
    if path is None:
        return None
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def git_state() -> dict:
    def run(*args):
        return subprocess.run(["git", *args], capture_output=True, text=True, check=False)
    head = run("rev-parse", "HEAD")
    dirty = run("status", "--porcelain")
    return {"commit": head.stdout.strip() if head.returncode == 0 else None,
            "dirty": bool(dirty.stdout.strip()) if dirty.returncode == 0 else None}


class Runner:
    def __init__(self, ports, raw_files, *, seconds, ap_ready_timeout, faults=(),
                 clock=time.monotonic, sleep=time.sleep, initial_reset=None, reopen=None,
                 events=None):
        self.ports = ports
        self.raw_files = raw_files
        self.seconds = seconds
        self.ap_ready_timeout = ap_ready_timeout
        self.faults = sorted(faults, key=lambda f: f[2])
        self.clock = clock
        self.sleep = sleep
        self.initial_reset = initial_reset
        self.reopen = reopen
        self.events: list[dict] = events if events is not None else []
        self.buffers = {role: b"" for role in ports}
        self.warning_counts = {role: {key: 0 for key in WARNING_PATTERNS} for role in ports}
        self.boot_index = {role: 0 for role in ports}
        self.line_number = {role: 0 for role in ports}
        self.ap_ready = False
        self.sta_started_at = None

    def _line(self, role: str, line: bytes):
        decoded = line.decode("utf-8", errors="replace")
        self.line_number[role] += 1
        for name, pattern in WARNING_PATTERNS.items():
            if pattern.lower() in decoded.lower():
                self.warning_counts[role][name] += 1
                emit(self.events, self.clock, "warning", role=role, code=name,
                     boot_index=self.boot_index[role], raw_line=self.line_number[role])
        for name, pattern in PANIC_PATTERNS.items():
            if pattern.lower() in decoded.lower():
                emit(self.events, self.clock, "panic", role=role, code=name,
                     boot_index=self.boot_index[role], raw_line=self.line_number[role])
        for parsed in validation.parse_lines(decoded):
            if parsed["marker"] == "RW_LINK_BOOT":
                self.boot_index[role] += 1
            emit(self.events, self.clock, "firmware", role=role, **parsed)
            if role == "ap" and parsed["marker"] == "RW_LINK_AP_READY":
                self.ap_ready = True
            print(f"[{role}] {parsed['marker']} {parsed['fields']}", flush=True)

    def poll(self):
        for role, port in self.ports.items():
            try:
                data = port.read(max(1, port.in_waiting))
            except OSError:
                if self.reopen is None:
                    raise
                try:
                    port.close()
                except OSError:
                    pass
                deadline = self.clock() + 10
                last_error = None
                while self.clock() < deadline:
                    try:
                        replacement = self.reopen(role)
                        self.ports[role] = replacement
                        emit(self.events, self.clock, "host_reopen", role=role)
                        break
                    except (OSError, RuntimeError) as exc:
                        last_error = exc
                        self.sleep(0.2)
                else:
                    raise RuntimeError(f"{role}: serial port did not reappear within 10s: {last_error}")
                continue
            if not data:
                continue
            self.raw_files[role].write(data)
            self.raw_files[role].flush()
            combined = self.buffers[role] + data
            *complete, tail = combined.split(b"\n")
            self.buffers[role] = tail[-8192:]
            for line in complete:
                self._line(role, line.rstrip(b"\r"))

    def _command(self, role: str, action: str):
        command = "RESTART" if action == "software_reset" else "AP_OFF_10S"
        fault = emit(self.events, self.clock, "fault", role=role, action=action,
                     command=command, acknowledged=False)
        payload = f"RW_LINK_CMD {command}\n".encode("ascii")
        try:
            written = self.ports[role].write(payload)
            if written is not None and written != len(payload):
                raise OSError(f"short serial write: {written}/{len(payload)} bytes")
        except Exception as exc:
            emit(self.events, self.clock, "error", message=f"{role}: {command} write failed: {exc}")
            raise
        deadline = self.clock() + 5
        while self.clock() < deadline:
            self.poll()
            if any(e.get("kind") == "firmware" and e.get("role") == role and
                   e.get("marker") == "RW_LINK_CMD_ACK" and
                   e.get("fields", {}).get("command") == command and
                   e.get("monotonic_s", -1) >= fault["monotonic_s"] for e in self.events):
                fault["acknowledged"] = True
                return
            self.sleep(0.05)
        emit(self.events, self.clock, "error", message=f"{role}: no ACK for {command}")

    def run(self) -> list[dict]:
        if self.initial_reset:
            for port in self.ports.values():
                flush = getattr(port, "reset_input_buffer", None)
                if callable(flush):
                    flush()
            emit(self.events, self.clock, "host_reset", role="ap", action="serial_reset")
            self.initial_reset(self.ports["ap"])
        gate_deadline = self.clock() + self.ap_ready_timeout
        while self.clock() < gate_deadline and not self.ap_ready:
            self.poll()
            self.sleep(0.02)
        if not self.ap_ready:
            emit(self.events, self.clock, "error", message="AP_READY gate timed out; STA was not started")
            self.stop_and_drain()
            return self.events
        if self.initial_reset:
            for role in ("sta", "sta2"):
                if role in self.ports:
                    emit(self.events, self.clock, "host_reset", role=role, action="serial_reset")
                    self.initial_reset(self.ports[role])
        self.sta_started_at = self.clock()
        emit(self.events, self.clock, "host", action="stations_started_after_ap_ready")
        deadline = self.sta_started_at + self.seconds
        next_fault = 0
        while self.clock() < deadline:
            self.poll()
            if next_fault < len(self.faults) and self.clock() - self.sta_started_at >= self.faults[next_fault][2]:
                role, action, _ = self.faults[next_fault]
                self._command(role, action)
                next_fault += 1
            self.sleep(0.02)
        self.stop_and_drain()
        for role, tail in self.buffers.items():
            if tail:
                emit(self.events, self.clock, "error", message=f"{role}: truncated final serial line")
        if next_fault != len(self.faults):
            emit(self.events, self.clock, "error", message="run ended before all scheduled faults")
        return self.events

    def _has_final_summary(self, role: str) -> bool:
        relevant = [e for e in self.events if e.get("kind") == "firmware" and e.get("role") == role
                    and e.get("marker") in {"RW_LINK_RUN_START", "RW_LINK_SUMMARY"}]
        return bool(relevant and relevant[-1].get("marker") == "RW_LINK_SUMMARY")

    def _complete(self, role: str) -> bool:
        relevant = [e for e in self.events if e.get("kind") == "firmware" and e.get("role") == role]
        starts = [e for e in relevant if e.get("marker") == "RW_LINK_RUN_START"]
        if not starts or not self._has_final_summary(role):
            return False
        end = starts[-1].get("monotonic_s", -1)
        markers = {e.get("marker") for e in relevant if e.get("monotonic_s", -1) >= end}
        return {"RW_LINK_RADIO_RESULT", "RW_LINK_RADIO_SHUTDOWN", "RW_LINK_DONE"} <= markers

    def stop_and_drain(self):
        # A role with a summary may already be in its shutdown path. It needs
        # time to print RESULT/SHUTDOWN/DONE rather than another STOP command.
        for role in ("sta", "sta2", "ap"):
            if role not in self.ports or self._has_final_summary(role):
                continue
            emit(self.events, self.clock, "host_stop", role=role, command="STOP")
            try:
                payload = b"RW_LINK_CMD STOP\n"
                written = self.ports[role].write(payload)
                if written is not None and written != len(payload):
                    raise OSError(f"short serial write: {written}/{len(payload)} bytes")
            except Exception as exc:
                emit(self.events, self.clock, "error", message=f"{role}: STOP write failed: {exc}")
        stop_deadline = self.clock() + 20
        while self.clock() < stop_deadline and not all(self._complete(r) for r in self.ports):
            try:
                self.poll()
            except Exception as exc:
                emit(self.events, self.clock, "error", message=f"cleanup capture failed: {exc}")
                break
            self.sleep(0.05)
        for role in self.ports:
            if not self._complete(role):
                emit(self.events, self.clock, "error", message=f"{role}: incomplete final summary/shutdown after STOP")


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--sta", choices=("COM5", "COM6"), required=True)
    p.add_argument("--sta2", choices=("COM6",), help="optional second STA (requires --sta COM5)")
    p.add_argument("--ap", choices=("COM4",), default="COM4")
    p.add_argument("--seconds", type=float, default=180)
    p.add_argument("--ap-ready-timeout", type=float, default=60)
    p.add_argument("--baud", type=int, default=115200)
    p.add_argument("--fault", action="append", type=parse_fault, default=[])
    p.add_argument("--output-dir", type=Path, required=True,
                   help="private local directory for complete raw logs and sanitized reports")
    p.add_argument("--public-log", type=Path, help="optional allowlisted RW_LINK telemetry copy")
    p.add_argument("--ap-bin", type=Path)
    p.add_argument("--sta-bin", type=Path)
    p.add_argument("--sta2-bin", type=Path)
    p.add_argument("--no-initial-reset", action="store_true")
    args = p.parse_args()
    if args.sta2 and args.sta != "COM5":
        p.error("--sta2 COM6 requires --sta COM5")
    if args.sta2_bin and not args.sta2:
        p.error("--sta2-bin requires --sta2 COM6")
    if any(role == "sta2" for role, _, _ in args.fault) and not args.sta2:
        p.error("sta2 faults require --sta2 COM6")
    if not math.isfinite(args.seconds) or args.seconds <= 0 or args.seconds > 3600:
        p.error("--seconds must be positive and at most 3600; this runner excludes the 8h soak")
    if not math.isfinite(args.ap_ready_timeout) or not 0 < args.ap_ready_timeout <= 300:
        p.error("--ap-ready-timeout must be 1..300 seconds")
    if any(at >= args.seconds for _, _, at in args.fault):
        p.error("fault offsets must be before the bounded run ends")
    for binary in (args.ap_bin, args.sta_bin, args.sta2_bin):
        if binary is not None and not binary.is_file():
            p.error(f"binary for SHA256 does not exist: {binary}")
    if args.output_dir.exists() and any(args.output_dir.iterdir()):
        p.error("--output-dir must be empty to avoid overwriting evidence")
    if ".private" not in args.output_dir.resolve().parts:
        p.error("--output-dir must be under a .private directory because it contains raw serial logs")
    args.output_dir.mkdir(parents=True, exist_ok=True)
    event_path = args.output_dir / "events.jsonl"
    events = EventJournal(event_path)
    ports = {}
    streams = {}
    runner = None
    try:
        import serial
        import serial.tools.list_ports
        devices = {"ap": args.ap, "sta": args.sta}
        if args.sta2:
            devices["sta2"] = args.sta2
        verify_inventory(serial, *devices.values())
        for role, name in devices.items():
            ports[role] = open_port(serial, name, args.baud)
            streams[role] = (args.output_dir / f"{role}-raw.log").open("wb")
        initial_reset = None
        if not args.no_initial_reset:
            from esp_idf_monitor.base.reset import Reset
            initial_reset = lambda port: Reset(port, "esp32s3").hard()
        runner = Runner(ports, streams, seconds=args.seconds,
                        ap_ready_timeout=args.ap_ready_timeout, faults=args.fault,
                        events=events,
                        initial_reset=initial_reset,
                        reopen=lambda role: (verify_device(serial, devices[role]),
                                             open_port(serial, devices[role], args.baud))[1])
        for role, name in devices.items():
            emit(runner.events, time.monotonic, "host_inventory", role=role,
                 port=name, mac=EXPECTED[name])
        events = runner.run()
    except (Exception, KeyboardInterrupt) as exc:
        if runner is not None:
            try:
                runner.stop_and_drain()
            except (Exception, KeyboardInterrupt) as cleanup_exc:
                emit(events, time.monotonic, "error",
                     message=f"cleanup interrupted or failed: {type(cleanup_exc).__name__}: {cleanup_exc}")
        emit(events, time.monotonic, "error", message=f"capture interrupted or failed: {type(exc).__name__}: {exc}")
    finally:
        for stream in streams.values():
            try:
                stream.close()
            except OSError as exc:
                emit(events, time.monotonic, "error", message=f"raw log close failed: {exc}")
        for port in ports.values():
            try:
                port.close()
            except Exception as exc:
                emit(events, time.monotonic, "error", message=f"serial close failed: {exc}")
    events.finalize()
    expected_roles = ("ap", "sta", "sta2") if args.sta2 else ("ap", "sta")
    report = validation.apply_acceptance(validation.analyze(events, expected_roles), events)
    report["manifest"] = {
        "ap": {"port": args.ap, "mac": EXPECTED[args.ap], "binary_sha256": sha256(args.ap_bin)},
        "sta": {"port": args.sta, "mac": EXPECTED[args.sta], "binary_sha256": sha256(args.sta_bin)},
        **({"sta2": {"port": args.sta2, "mac": EXPECTED[args.sta2],
                    "binary_sha256": sha256(args.sta2_bin)}} if args.sta2 else {}),
        "host_git": git_state(), "bounded_seconds": args.seconds,
        "warning_counts": runner.warning_counts if runner else None,
        "warning_locations": [e for e in events if e.get("kind") == "warning"],
        "raw_log_directory": str(args.output_dir.resolve()),
    }
    (args.output_dir / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    (args.output_dir / "report.md").write_text(validation.markdown(report), encoding="utf-8")
    if args.public_log:
        args.public_log.parent.mkdir(parents=True, exist_ok=True)
        args.public_log.write_text("".join(
            f"{e['iso_time']} {e['role']} {e['marker']} " +
            " ".join(f"{k}={v}" for k, v in e["fields"].items()) + "\n"
            for e in events if e.get("kind") == "firmware"), encoding="utf-8")
    print(f"{report['status']}: {args.output_dir / 'report.md'}")
    return 0 if report["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
