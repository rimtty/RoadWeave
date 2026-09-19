"""Offline counter reconciliation and bounded throughput policy checks."""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import halow_throughput as t
from halow_throughput_run import ThroughputRunner, command, markdown


def fw(role, marker, stage, **fields):
    return {"kind": "firmware", "role": role, "marker": marker,
            "fields": {"stage": str(stage), **{k: str(v) for k, v in fields.items()}}}


def stage_fixture(*, sent=400, active=400, late=0, rate=128, duration_us=30_000_000):
    stage = 7
    events = [fw("ap", "RW_TPUT_ARM_ACK", stage, peer="192.168.50.2",
                 duration_s=30, packet_bytes=1200)]
    events += [fw("ap", marker, stage) for marker in (
        "RW_TPUT_RX_READY", "RW_TPUT_RX_START", "RW_TPUT_RX_DRAIN")]
    events += [fw("sta", "RW_TPUT_SEND_ACK", stage, peer="192.168.50.1",
                  duration_s=30, rate_kbps=rate, packet_bytes=1200)]
    events += [fw("sta", marker, stage) for marker in ("RW_TPUT_TX_START", "RW_TPUT_TX_END")]
    events.append(fw("sta", "RW_TPUT_TX_SUMMARY", stage, packet_bytes=1200,
                     body_bytes=1184, rate_kbps=rate, requested_ms=30000, attempts=sent,
                     sent=sent, send_fail=0, sent_udp_bytes=sent * 1200,
                     sent_body_bytes=sent * 1184, duration_us=duration_us,
                     start_us=1_000_000, end_us=1_000_000 + duration_us,
                     start_acked=1, end_acked=1, aborted=0))
    events.append(fw("ap", "RW_TPUT_RX_SUMMARY", stage, packet_bytes=1200,
                     body_bytes=1184, active_unique=active, late_unique=late,
                     drain_unique=late, all_unique=active + late, duplicate=0,
                     invalid=0, wrong_stage=0, wrong_peer=0, overflow=0,
                     first_active_us=1_000_000, last_active_us=30_500_000,
                     active_span_us=29_500_000, drain_start_us=32_000_000,
                     drain_end_us=35_100_000,
                     active_body_bytes=active * 1184,
                     all_body_bytes=(active + late) * 1184, end_sent=sent,
                     drain_sent=sent, start_seen=1, end_seen=1,
                     drain_command=1, count_match=1, aborted=0))
    return events


def assess(events, rate=128):
    return t.assess_stage(events, stage=7, sender="sta", receiver="ap",
                          rate_kbps=rate, requested_s=30, phase="ramp")


class ThroughputTests(unittest.TestCase):
    def test_parser_only_retains_known_fields_in_coalesced_serial(self):
        row = t.parse_lines("driver 8379RW_TPUT_TX_SUMMARY stage=7 sent=400 psk=secret"
                            "RW_TPUT_RX_SUMMARY stage=7 all_unique=400")
        self.assertEqual(len(row), 2)
        self.assertEqual(row[0]["fields"], {"stage": "7", "sent": "400"})

    def test_parser_preserves_stage_diagnostics_without_unknown_fields(self):
        rows = t.parse_lines(
            "RW_TPUT_TX_DIAG stage=7 fill_us=500 send_us=2000 "
            "send_max_us=1100 send_over_1ms=1 send_over_10ms=0 "
            "pace_wait_us=300 pace_waits=2 pace_late_us=40 "
            "pace_late_max_us=30 loop_yield_us=100 psk=secret"
            "RW_TPUT_RX_DIAG stage=7 recv_us=4000 recv_max_us=1000 "
            "recv_timeouts=2 process_us=200 process_max_us=20 "
            "verify_us=100 verify_max_us=10 verify_packets=8 loop_yield_us=50")
        self.assertEqual([row["marker"] for row in rows],
                         ["RW_TPUT_TX_DIAG", "RW_TPUT_RX_DIAG"])
        self.assertEqual(rows[0]["fields"]["send_us"], "2000")
        self.assertEqual(rows[1]["fields"]["process_us"], "200")
        self.assertEqual(rows[1]["fields"]["verify_packets"], "8")
        self.assertNotIn("psk", rows[0]["fields"])

    def test_exact_valid_stage_and_body_goodput(self):
        result = assess(stage_fixture())
        self.assertTrue(result["valid"], result["errors"])
        self.assertTrue(result["sustainable"])
        self.assertEqual(result["active_body_goodput_bps"], 400 * 1184 * 8 / 30)
        self.assertEqual(result["actual_tx_bps"], 128000)

    def test_late_drain_cannot_rescue_active_delivery(self):
        result = assess(stage_fixture(sent=400, active=380, late=20))
        self.assertTrue(result["valid"], result["errors"])
        self.assertFalse(result["sustainable"])
        self.assertEqual(result["final_delivery"], 1)
        self.assertEqual(result["active_delivery"], .95)

    def test_duplicate_is_excluded_but_not_automatic_failure(self):
        events = stage_fixture()
        events[-1]["fields"]["duplicate"] = "9"
        result = assess(events)
        self.assertTrue(result["sustainable"], result["errors"])
        self.assertEqual(result["duplicates"], 9)

    def test_end_mismatch_and_missing_end_are_invalid(self):
        events = stage_fixture()
        events[-1]["fields"]["end_sent"] = "401"
        self.assertIn("stage 7: END/DRAIN sent count differs from TX summary", assess(events)["errors"])
        events = stage_fixture()
        events[-1]["fields"]["end_seen"] = "0"
        self.assertFalse(assess(events)["valid"])

    def test_invalid_overflow_or_stage_mix_is_invalid(self):
        for field in ("invalid", "wrong_stage", "wrong_peer", "overflow"):
            with self.subTest(field=field):
                events = stage_fixture()
                events[-1]["fields"][field] = "1"
                self.assertFalse(assess(events)["valid"])

    def test_truncated_or_unknown_counters_never_pass(self):
        events = stage_fixture()
        events[-1]["fields"].pop("all_unique")
        self.assertFalse(assess(events)["valid"])
        events = stage_fixture()
        events[-1]["fields"]["all_unique"] = "NA"
        self.assertFalse(assess(events)["valid"])

    def test_sender_rate_and_duration_gate(self):
        result = assess(stage_fixture(sent=300, active=300))
        self.assertTrue(result["valid"])
        self.assertFalse(result["sustainable"])
        events = stage_fixture(duration_us=25_000_000)
        self.assertFalse(assess(events)["valid"])

    def test_unpaced_is_observation_only(self):
        result = assess(stage_fixture(rate=0), rate=0)
        self.assertTrue(result["valid"])
        self.assertFalse(result["sustainable"])

    def test_command_rejects_short_serial_write(self):
        class ShortPort:
            def write(self, data):
                return len(data) - 1

        with self.assertRaisesRegex(OSError, "short serial command write"):
            command(ShortPort(), "ARM stage=1", [], lambda: 1., "ap")


class SearchTests(unittest.TestCase):
    def test_stage_wait_reuses_summary_captured_with_drain_ack(self):
        events = [
            {"kind": "firmware", "role": "ap", "marker": "RW_TPUT_CMD_ACK",
             "fields": {"stage": "7", "command": "DRAIN"}, "monotonic_s": 10},
            {"kind": "firmware", "role": "ap", "marker": "RW_TPUT_RX_SUMMARY",
             "fields": {"stage": "7", "all_unique": "400"}, "monotonic_s": 10},
        ]
        runner = ThroughputRunner({}, {}, direction="sta_to_ap", startup_timeout=5,
                                  max_seconds=100, warmup_seconds=5, stage_seconds=5,
                                  repeat_seconds=5, rates=(128,), events=events)
        runner.deadline = runner.clock() + 100
        runner.poll = lambda: self.fail("already captured stage marker should not poll again")
        ack = runner._wait("ap", "RW_TPUT_CMD_ACK", stage=7, timeout=5)
        summary = runner._wait("ap", "RW_TPUT_RX_SUMMARY", stage=7, timeout=15)
        self.assertEqual(ack["fields"]["command"], "DRAIN")
        self.assertEqual(summary["fields"]["all_unique"], "400")

    def test_measure_waits_for_drain_ack_then_bounded_summary(self):
        class Controlled(ThroughputRunner):
            def _send(self, role, payload):
                self.commands.append((role, payload))

            def _wait(self, role, marker, *, stage=None, timeout):
                self.waits.append((role, marker, stage, timeout))
                if marker == "RW_TPUT_CMD_ACK":
                    return {"fields": {"command": "DRAIN"}}
                if marker == "RW_TPUT_TX_SUMMARY":
                    return {"fields": {"sent": "400"}}
                return {"fields": {}}

        runner = Controlled({}, {}, direction="sta_to_ap", startup_timeout=5,
                            max_seconds=1000, warmup_seconds=5, stage_seconds=30,
                            repeat_seconds=60, rates=(128,), events=stage_fixture())
        runner.stage_number = 6
        runner.deadline = runner.clock() + 1000
        runner.ips = {"ap": "192.168.50.1", "sta": "192.168.50.2"}
        runner.commands = []
        runner.waits = []
        result = runner.measure(128, 30, "ramp")
        self.assertTrue(result["valid"], result["errors"])
        self.assertEqual([wait[1] for wait in runner.waits],
                         ["RW_TPUT_RX_READY", "RW_TPUT_TX_SUMMARY",
                          "RW_TPUT_CMD_ACK", "RW_TPUT_RX_SUMMARY"])
        self.assertEqual(runner.waits[-1][-1], 15)

    def test_smoke_markdown_uses_actual_confirmation_duration(self):
        rendered = markdown({"status": "FAIL", "direction": "sta_to_ap",
                             "bandwidth_mhz": 1, "stages": [], "confirmed": None,
                             "repeat_seconds": 5, "errors": []})
        self.assertIn("No 3×5 s sustainable rate", rendered)

    def test_shutdown_evidence_distinguishes_failure_from_missing_markers(self):
        events = [{"kind": "host_command", "role": "ap", "command": "STOP", "monotonic_s": 10}]
        runner = ThroughputRunner({}, {}, direction="sta_to_ap", startup_timeout=5,
                                  max_seconds=100, warmup_seconds=5, stage_seconds=5,
                                  repeat_seconds=5, rates=(128,), events=events)
        self.assertEqual(runner._shutdown_evidence("ap"), (False, False))
        for marker, fields in (("RW_TPUT_CMD_ACK", {"command": "STOP"}),
                               ("RW_TPUT_SESSION_END", {"stopped": "1", "all_stages_ok": "1"}),
                               ("RW_LINK_RADIO_SHUTDOWN", {}),
                               ("RW_LINK_RADIO_RESULT", {"value": "FAIL"}),
                               ("RW_LINK_DONE", {})):
            events.append({"kind": "firmware", "role": "ap", "marker": marker,
                           "fields": fields, "monotonic_s": 11})
        self.assertEqual(runner._shutdown_evidence("ap"), (True, False))
        events[-2]["fields"]["value"] = "PASS"
        self.assertEqual(runner._shutdown_evidence("ap"), (True, True))

    def make_runner(self, *, repeat_fails=()):
        class Clock:
            t = 0.

            def __call__(self):
                return self.t

            def sleep(self, seconds):
                self.t += seconds

        class MockSearch(ThroughputRunner):
            def _startup(self):
                self.deadline = self.clock() + 1000

            def measure(self, rate_kbps, seconds, phase):
                self.stage_number += 1
                self.sleep(seconds + 3)
                sustainable = rate_kbps > 0 and rate_kbps <= 256 and not (
                    phase == "confirm" and rate_kbps in repeat_fails)
                row = {"stage": self.stage_number, "phase": phase,
                       "target_kbps": rate_kbps, "valid": True,
                       "sustainable": sustainable,
                       "active_body_goodput_bps": rate_kbps * 800}
                self.stages.append(row)
                return row

        clock = Clock()
        return MockSearch({}, {}, direction="sta_to_ap", startup_timeout=5,
                          max_seconds=1000, warmup_seconds=5, stage_seconds=30,
                          repeat_seconds=60, rates=(128, 256, 512),
                          clock=clock, sleep=clock.sleep)

    def test_ramp_refines_then_confirms_three_times(self):
        runner = self.make_runner()
        result = runner.run_search()
        self.assertEqual(result["confirmed"]["target_kbps"], 256)
        self.assertEqual([s["target_kbps"] for s in runner.stages if s["phase"] == "refine"],
                         [384, 320])
        self.assertEqual(len([s for s in runner.stages if s["phase"] == "confirm"]), 3)
        self.assertEqual(result["saturation"]["target_kbps"], 0)

    def test_failed_confirmation_downgrades_and_repeats(self):
        runner = self.make_runner(repeat_fails={256})
        result = runner.run_search()
        self.assertEqual(result["confirmed"]["target_kbps"], 128)
        self.assertEqual([s["target_kbps"] for s in runner.stages if s["phase"] == "confirm"],
                         [256] * 3 + [128] * 3)


if __name__ == "__main__":
    unittest.main()
