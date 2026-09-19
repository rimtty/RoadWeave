import sys
import copy
import json
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import halow_throughput_analyze as analysis


class AnalyzeTests(unittest.TestCase):
    def test_published_case_reconciles_and_detects_counter_tampering(self):
        repo = Path(__file__).resolve().parents[2]
        events = repo / "docs/bringup/logs/2026-09-19-throughput"
        metrics = json.loads((events / "metrics.json").read_text(encoding="utf-8"))
        row = next(c for c in metrics["cases"] if c["case"] == "final-2mhz-sta-to-ap")
        case = {"name": row["case"], "report": row}
        self.assertEqual(analysis.reconcile_events(case, events), [])
        changed = copy.deepcopy(case)
        changed["report"]["stages"][0]["tx"]["send_fail"] += 1
        self.assertTrue(any("saved tx differs" in error for error in
                            analysis.reconcile_events(changed, events)))

    def test_max_observed_is_separate_from_confirmed_and_excludes_failed(self):
        case = {"name": "example", "source": "report.json", "report": {
            "status": "PASS", "mode": "full", "stages": [
                {"stage": 1, "phase": "warmup", "valid": True,
                 "active_payload_goodput_bps": 9_000_000, "active_body_goodput_bps": 8_000_000},
                {"stage": 2, "phase": "ramp", "valid": False,
                 "active_payload_goodput_bps": 8_000_000, "active_body_goodput_bps": 7_000_000},
                {"stage": 3, "phase": "saturation", "valid": True,
                 "active_payload_goodput_bps": 3_100_000, "active_body_goodput_bps": 3_050_000}],
            "confirmed": {"target_kbps": 2500}}}
        result = analysis.summarize(case)
        self.assertEqual(result["confirmed_kbps"], 2500)
        self.assertEqual(result["maximum_observed"]["stage"], 3)
        case["report"]["status"] = "FAIL"
        self.assertIsNone(analysis.summarize(case)["maximum_observed"])

    def test_stage_flags_distinguish_sender_and_receiver(self):
        base = {"stage": 2, "phase": "ramp", "valid": True, "sustainable": False,
                "target_kbps": 128, "actual_tx_bps": 100_000,
                "active_delivery": .88, "final_delivery": .90,
                "tx": {"sent": 100, "send_fail": 0}, "rx": {"all_unique": 90}}
        summary = analysis.stage_summary(base)
        self.assertIn("tx_rate_below_target", summary["flags"])
        self.assertIn("rf_or_receive_loss", summary["flags"])
        self.assertIn("low_load_loss", summary["flags"])
        self.assertIn("late_delivery", summary["flags"])
        self.assertNotIn("invalid_evidence", summary["flags"])

    def test_first_failed_stage_does_not_claim_capacity(self):
        case = {"name": "sample", "source": "sample.json", "report": {
            "status": "FAIL", "stages": [{"stage": 1, "phase": "ramp", "valid": True,
                                     "sustainable": False, "target_kbps": 128}]}}
        summary = analysis.summarize(case)
        self.assertEqual(summary["first_failed_search_stage"], 1)
        self.assertFalse(summary["first_failure_is_capacity_bound"])

    def test_binary_provenance_required_for_comparison(self):
        first = {"name": "a", "direction": "sta_to_ap", "bandwidth_mhz": 1,
                 "confirmed_kbps": 128, "provenance": {"firmware_source": "x",
                 "ap_binary_sha256": "a", "sta_binary_sha256": "s"}}
        second = {**first, "name": "b", "confirmed_kbps": 256}
        self.assertEqual(analysis.compare([first, second])[0]["confirmed_delta_kbps"], 128)
        second = {**second, "provenance": {**first["provenance"], "sta_binary_sha256": "other"}}
        self.assertFalse(analysis.compare([first, second])[0]["comparable"])
        self.assertIsNone(analysis.compare([first, second])[0]["confirmed_delta_kbps"])


if __name__ == "__main__":
    unittest.main()
