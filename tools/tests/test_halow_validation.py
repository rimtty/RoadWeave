"""Offline checks for the HaLow serial parser and bounded runner."""

import json
import copy
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


def three_events():
    events = good_events()
    for event in events:
        if event["role"] == "ap" and event["marker"] == "RW_LINK_RUN_START":
            event["fields"].update(id="0", ip="192.168.50.1", max_stas="2")
        if event["role"] == "sta" and event["marker"] == "RW_LINK_RUN_START":
            event["fields"].update(id="1", ip="192.168.50.2", run_id="one")
        if event["role"] == "ap" and event["marker"] == "RW_LINK_SUMMARY":
            event["fields"]["echo_id2"] = "240"
    sta2 = [copy.deepcopy(e) for e in events if e["role"] == "sta"]
    for event in sta2:
        event["role"] = "sta2"
        if event["marker"] == "RW_LINK_RUN_START":
            event["fields"].update(id="2", ip="192.168.50.3", run_id="two")
    inventory = [{"kind": "host_inventory", "role": "ap", "port": "COM4", "monotonic_s": -1},
                 {"kind": "host_inventory", "role": "sta", "port": "COM5", "monotonic_s": -1},
                 {"kind": "host_inventory", "role": "sta2", "port": "COM6", "monotonic_s": -1}]
    return sorted(inventory + events + sta2, key=lambda e: e["monotonic_s"])


def three_sta_restart_events():
    events = three_events()
    events = [e for e in events if not (e["role"] == "sta" and e.get("marker") == "RW_LINK_ECHO_MATCH" and e["monotonic_s"] >= 40)]
    events = [e for e in events if not (e["role"] == "sta" and e.get("marker") == "RW_LINK_SAMPLE")]
    events += [fw("sta", "RW_LINK_BOOT", 41, reset_reason=3),
               fw("sta", "RW_LINK_RUN_START", 42, id=1, ip="192.168.50.2", run_id="after-reset")]
    events += [fw("sta", "RW_LINK_ECHO_MATCH", 45 + i * .5, seq=i + 1, rtt_us=25000) for i in range(80)]
    events.append({"kind": "fault", "role": "sta", "action": "software_reset",
                   "monotonic_s": 40, "acknowledged": True})
    summary = next(e for e in events if e["role"] == "sta" and e.get("marker") == "RW_LINK_SUMMARY")
    summary["fields"].update(sent="80", received="80", lost="0")
    return sorted(events, key=lambda e: e["monotonic_s"])


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
        self.assertIn("sta baseline: post-warmup valid echo delivery below 99.0%", report["errors"])

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

    def test_three_node_independent_baseline_and_ap_peer_counters(self):
        events = three_events()
        report = v.apply_acceptance(v.analyze(events), events)
        self.assertEqual(report["status"], "PASS", report["errors"])
        self.assertEqual(report["post_warmup_by_role"]["sta2"]["sent"], 220)
        summary = next(e for e in events if e["role"] == "ap" and e.get("marker") == "RW_LINK_SUMMARY")
        summary["fields"]["echo_id2"] = "10"
        self.assertIn("three-node: AP echo_id2 does not cover sta2 exact replies", v.analyze(events)["errors"])

    def test_three_node_missing_second_summary_or_duplicate_ip_fails(self):
        events = [e for e in three_events() if not (e["role"] == "sta2" and e.get("marker") == "RW_LINK_SUMMARY")]
        self.assertIn("sta2: missing RW_LINK_SUMMARY", v.analyze(events)["errors"])
        events = three_events()
        start2 = next(e for e in events if e["role"] == "sta2" and e.get("marker") == "RW_LINK_RUN_START")
        start2["fields"]["ip"] = "192.168.50.2"
        self.assertIn("three-node: STA IPs are missing or identical", v.analyze(events)["errors"])
        events = three_events()
        start2 = next(e for e in events if e["role"] == "sta2" and e.get("marker") == "RW_LINK_RUN_START")
        start2["fields"]["id"] = "1"
        start2["fields"]["run_id"] = "one"
        errors = v.analyze(events)["errors"]
        self.assertIn("sta2: firmware ID does not match COM6", errors)
        self.assertIn("three-node: STA run IDs are missing or identical", errors)

    def test_three_node_requires_shared_measurement_window(self):
        events = three_events()
        for event in events:
            if event["role"] == "sta2" and event.get("kind") == "firmware":
                event["monotonic_s"] += 30
        report = v.apply_acceptance(v.analyze(events), events)
        self.assertIn("three-node: simultaneous post-warmup interval shorter than 120s", report["errors"])

    def test_three_node_dhcp_addresses_from_lease_and_bind(self):
        self.assertEqual(v.parse_lines("RW_LINK_DHCP_CLIENT=ENABLED status=1")[-1]["fields"],
                         {"value": "ENABLED", "status": "1"})
        events = three_events()
        events.append(fw("ap", "RW_LINK_DHCP_SERVER", 0.5, value="STARTED"))
        for role, peer_id, ip in (("sta", 1, "192.168.50.22"), ("sta2", 2, "192.168.50.23")):
            start = next(e for e in events if e["role"] == role and e.get("marker") == "RW_LINK_RUN_START")
            start["fields"]["ip"] = "DHCP"
            events += [fw(role, "RW_LINK_DHCP_CLIENT", 1.0, value="ENABLED", status=1),
                       fw(role, "RW_LINK_DHCP_LEASE", 1.1, ip=ip, gw="192.168.50.1", acquire_ms=500),
                       fw(role, "RW_LINK_UDP_BIND", 1.5, ip=ip)]
            mac = f"02:00:00:00:00:0{peer_id}"
            events.append(fw("ap", "RW_LINK_DHCP_LEASE", 2.5, ip=ip, mac=mac))
            events.append(fw("ap", "RW_LINK_AP_PEER", 3, id=peer_id, ip=ip, mac=mac))
        report = v.apply_acceptance(v.analyze(events), events)
        self.assertEqual(report["status"], "PASS", report["errors"])
        self.assertEqual(report["roles"]["sta2"]["effective_ip"], "192.168.50.23")

    def test_two_node_dhcp_evidence_is_independent_of_three_node_mode(self):
        events = good_events()
        start = next(e for e in events if e["role"] == "sta" and e["marker"] == "RW_LINK_RUN_START")
        start["fields"].update(id="1", ip="DHCP")
        ip, mac = "192.168.50.22", "02:00:00:00:00:01"
        events += [fw("ap", "RW_LINK_DHCP_SERVER", .5, value="STARTED"),
                   fw("sta", "RW_LINK_DHCP_CLIENT", 1, value="ENABLED", status=1),
                   fw("sta", "RW_LINK_DHCP_LEASE", 1.2, ip=ip),
                   fw("sta", "RW_LINK_UDP_BIND", 1.3, ip=ip),
                   fw("ap", "RW_LINK_DHCP_LEASE", 2, ip=ip, mac=mac),
                   fw("ap", "RW_LINK_AP_PEER", 3, id=1, ip=ip, mac=mac)]
        report = v.apply_acceptance(v.analyze(events), events)
        self.assertEqual(report["status"], "PASS", report["errors"])
        self.assertEqual(report["roles"]["sta"]["ip_mode"], "dhcp")

    def test_three_node_restart_other_station_continues(self):
        events = three_sta_restart_events()
        report = v.apply_acceptance(v.analyze(events), events)
        self.assertEqual(report["status"], "PASS", report["errors"])
        self.assertGreater(report["faults"][0]["unaffected_exact_echoes_during_restart"], 0)
        events = [e for e in events if not (e["role"] == "sta2" and e.get("marker") == "RW_LINK_ECHO_MATCH" and 42 <= e["monotonic_s"] <= 50)]
        summary = next(e for e in events if e["role"] == "sta2" and e.get("marker") == "RW_LINK_SUMMARY")
        received = sum(e["role"] == "sta2" and e.get("marker") == "RW_LINK_ECHO_MATCH" for e in events)
        summary["fields"].update(received=str(received), lost=str(240 - received))
        report = v.apply_acceptance(v.analyze(events), events)
        self.assertIn("fault 1: sta2 echo gap exceeded 5s during sta restart", report["errors"])


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

    def test_three_port_gate_reset_and_stop_all(self):
        clock = FakeClock()
        ports = {"ap": FakePort(b"RW_LINK_AP_READY ip=192.168.50.1\n"),
                 "sta": FakePort(), "sta2": FakePort()}
        with tempfile.TemporaryDirectory() as d:
            files = {r: (Path(d) / f"{r}.log").open("wb") for r in ports}
            try:
                runner = Runner(ports, files, seconds=.1, ap_ready_timeout=.2,
                                clock=clock, sleep=clock.sleep,
                                initial_reset=lambda port: port.write(b"RESET"))
                runner.run()
            finally:
                for f in files.values():
                    f.close()
        self.assertEqual(ports["sta"].writes, [b"RESET", b"RW_LINK_CMD STOP\n"])
        self.assertEqual(ports["sta2"].writes, [b"RESET", b"RW_LINK_CMD STOP\n"])


if __name__ == "__main__":
    unittest.main()
