"""Offline checks for the HaLow serial parser and bounded runner."""

import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import halow_validation as v
from halow_validation_run import Runner


def fw(role, marker, t, **fields):
    return {"kind": "firmware", "role": role, "marker": marker,
            "fields": {k: str(value) for k, value in fields.items()}, "monotonic_s": t}


def good_events():
    events = [fw("ap", "RW_LINK_RUN_START", 0),
              fw("ap", "RW_LINK_AP_READY", 1),
              fw("sta", "RW_LINK_RUN_START", 2)]
    events.extend(fw("sta", "RW_LINK_ECHO_MATCH", 3 + i * .5, seq=i, rtt_us=25000)
                  for i in range(240))
    events.extend([fw("sta", "RW_LINK_SAMPLE", 12, elapsed_ms=10000, sent=20, received=20, lost=0),
                   fw("ap", "RW_LINK_SAMPLE", 62, elapsed_ms=62000, echo_id1=120, echo_id2=0, heap_free=100000),
                   fw("sta", "RW_LINK_SAMPLE", 62, sent=120, received=120, lost=0, heap_free=100000),
                   fw("ap", "RW_LINK_SUMMARY", 135, echo_id1=240, echo_id2=0),
                   fw("ap", "RW_LINK_RUN_END", 135),
                   fw("ap", "RW_LINK_RADIO_RESULT", 136, value="PASS"),
                   fw("ap", "RW_LINK_RADIO_SHUTDOWN", 136),
                   fw("ap", "RW_LINK_DONE", 136),
                   fw("sta", "RW_LINK_SUMMARY", 135, sent=240, received=240, lost=0),
                   fw("sta", "RW_LINK_RUN_END", 135),
                   fw("sta", "RW_LINK_RADIO_RESULT", 136, value="PASS"),
                   fw("sta", "RW_LINK_RADIO_SHUTDOWN", 136),
                   fw("sta", "RW_LINK_DONE", 136)])
    return events


class ParserTests(unittest.TestCase):
    def test_allowlist_discards_credentials_and_parses_prefixed_line(self):
        self.assertIsNone(v.parse_line("I (100) SSID=secret PSK=secret"))
        event = v.parse_line("I (100) LINK: RW_LINK_SAMPLE sent=4 received=3 psk=secret heap_free=800")
        self.assertEqual(event, {"marker": "RW_LINK_SAMPLE", "fields": {"sent": "4", "received": "3", "heap_free": "800"}})

    def test_coalesced_driver_prefix_and_multiple_markers(self):
        self.assertEqual(v.parse_lines("...timeout 8379RW_LINK_ECHO_MATCH seq=16 rtt_us=11601"),
                         [{"marker": "RW_LINK_ECHO_MATCH", "fields": {"seq": "16", "rtt_us": "11601"}}])
        self.assertEqual(v.parse_lines("8379RW_LINK_RADIO_RESULT=PASSRW_LINK_DONE"),
                         [{"marker": "RW_LINK_RADIO_RESULT", "fields": {"value": "PASS"}},
                          {"marker": "RW_LINK_DONE", "fields": {}}])

    def test_good_complete_baseline(self):
        report = v.apply_acceptance(v.analyze(good_events()), good_events())
        self.assertEqual(report["status"], "PASS", report["errors"])

    def test_flash_autoboot_before_managed_sta_reset_is_excluded(self):
        events = good_events()
        events += [fw("sta", "RW_LINK_BOOT", .2, reset_reason=11),
                   fw("sta", "RW_LINK_RUN_START", .4, id=1),
                   fw("sta", "RW_LINK_ECHO_MATCH", .8, seq=999, rtt_us=1000),
                   {"kind": "host_reset", "role": "ap", "action": "serial_reset", "monotonic_s": 0},
                   {"kind": "host_reset", "role": "sta", "action": "serial_reset", "monotonic_s": 1.5}]
        report = v.apply_acceptance(v.analyze(events), events)
        self.assertEqual(report["status"], "PASS", report["errors"])
        self.assertEqual(report["ignored_prestart_events"], 3)
        events.append(fw("sta", "RW_LINK_BOOT", 50, reset_reason=7))
        self.assertIn("sta: unexpected or missing post-start boot count", v.analyze(events)["errors"])

    def test_missing_summary_and_missing_matches_fail(self):
        events = good_events()
        events = [e for e in events if not (e["role"] == "sta" and e["marker"] == "RW_LINK_SUMMARY")]
        self.assertEqual(v.analyze(events)["status"], "FAIL")
        events = good_events()
        events = [e for e in events if not (e["role"] == "sta" and e["marker"] == "RW_LINK_ECHO_MATCH" and e["fields"]["seq"] == "5")]
        self.assertIn("sta: final run logged valid echo count differs from summary received", v.analyze(events)["errors"])

    def test_delivery_gate_and_short_run(self):
        events = good_events()
        summary = next(e for e in events if e["role"] == "sta" and e["marker"] == "RW_LINK_SUMMARY")
        summary["fields"]["received"] = "200"
        summary["fields"]["lost"] = "40"
        report = v.apply_acceptance(v.analyze(events), events)
        self.assertIn("baseline: post-warmup valid echo delivery below 99.0%", report["errors"])

    def test_truncated_jsonl_fails(self):
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / "events.jsonl"
            path.write_text(json.dumps(good_events()[0]) + "\n{" , encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "truncated"):
                v.load_events(path)

    def test_unacknowledged_fault_cannot_pass(self):
        events = good_events()
        events.append({"kind": "fault", "role": "ap", "action": "software_reset",
                       "monotonic_s": 30, "acknowledged": False})
        self.assertEqual(v.analyze(events)["status"], "FAIL")

    def test_firmware_failure_and_incomplete_shutdown_fail(self):
        events = good_events()
        events[-3]["fields"]["value"] = "FAIL"
        self.assertEqual(v.analyze(events)["status"], "FAIL")
        events = [e for e in good_events() if not (e["role"] == "ap" and e["marker"] == "RW_LINK_DONE")]
        self.assertIn("ap: missing final RW_LINK_DONE", v.analyze(events)["errors"])

    def test_actual_firmware_ap_sample_schema(self):
        parsed = v.parse_line("RW_LINK_SAMPLE role=AP elapsed_ms=10000 echo_id1=42 echo_id2=0 invalid=0 send_fail=0 useful_bps=8000 heap_free=90000 heap_min=88000 snr_db=NA rc_sent_start=NA")
        self.assertEqual(parsed["fields"]["echo_id1"], "42")
        self.assertEqual(parsed["fields"]["elapsed_ms"], "10000")
        self.assertEqual(parsed["fields"]["rc_sent_start"], "NA")

    def test_sta_reset_requires_fresh_boot_not_recovery_marker(self):
        events = good_events()
        events += [{"kind": "fault", "role": "sta", "action": "software_reset",
                    "monotonic_s": 40, "acknowledged": True}]
        events += [fw("sta", "RW_LINK_BOOT", 41, reset_reason=3)]
        self.assertNotIn("sta: no recovery marker after AP fault", v.analyze(events)["errors"])

    def test_wrong_reset_reason_and_unplanned_boot_fail(self):
        events = good_events()
        events.append(fw("sta", "RW_LINK_BOOT", 40, reset_reason=7))
        self.assertIn("sta: unexpected or missing post-start boot count", v.analyze(events)["errors"])
        events.append({"kind": "fault", "role": "sta", "action": "software_reset",
                       "monotonic_s": 39, "acknowledged": True})
        self.assertIn("sta: boot after software reset did not report software reset", v.analyze(events)["errors"])

    def test_raw_panic_fails(self):
        events = good_events()
        events.append({"kind": "panic", "role": "ap", "code": "guru_meditation"})
        self.assertEqual(v.analyze(events)["status"], "FAIL")


class FakeClock:
    def __init__(self):
        self.t = 0.

    def __call__(self):
        return self.t

    def sleep(self, seconds):
        self.t += seconds


class FakePort:
    def __init__(self, data=b""):
        self.data = data
        self.writes = []

    @property
    def in_waiting(self):
        return len(self.data)

    def read(self, count):
        chunk, self.data = self.data[:count], self.data[count:]
        return chunk

    def write(self, data):
        self.writes.append(data)


class RunnerTests(unittest.TestCase):
    def test_ap_gate_prevents_sta_start_on_timeout(self):
        clock = FakeClock()
        ports = {"ap": FakePort(), "sta": FakePort()}
        with tempfile.TemporaryDirectory() as d:
            files = {r: (Path(d) / f"{r}.log").open("wb") for r in ports}
            try:
                runner = Runner(ports, files, seconds=2, ap_ready_timeout=0.2,
                                clock=clock, sleep=clock.sleep,
                                initial_reset=lambda port: port.write(b"RESET"))
                events = runner.run()
            finally:
                for f in files.values():
                    f.close()
        self.assertEqual(ports["ap"].writes, [b"RESET", b"RW_LINK_CMD STOP\n"])
        self.assertEqual(ports["sta"].writes, [b"RW_LINK_CMD STOP\n"])
        self.assertTrue(any(e.get("kind") == "error" for e in events))

    def test_fault_command_and_raw_capture(self):
        clock = FakeClock()
        ports = {"ap": FakePort(b"RW_LINK_AP_READY ip=192.168.50.1\n"),
                 "sta": FakePort()}
        with tempfile.TemporaryDirectory() as d:
            files = {r: (Path(d) / f"{r}.log").open("wb") for r in ports}
            try:
                runner = Runner(ports, files, seconds=.5, ap_ready_timeout=.2,
                                faults=[("ap", "software_reset", .1)],
                                clock=clock, sleep=clock.sleep)
                events = runner.run()
            finally:
                for f in files.values():
                    f.close()
            self.assertIn(b"RW_LINK_AP_READY", (Path(d) / "ap.log").read_bytes())
        self.assertIn(b"RW_LINK_CMD RESTART\n", ports["ap"].writes)
        self.assertTrue(any(e.get("kind") == "fault" and not e["acknowledged"] for e in events))


if __name__ == "__main__":
    unittest.main()
