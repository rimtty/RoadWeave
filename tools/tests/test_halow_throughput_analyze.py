import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import halow_throughput_analyze as analysis


class AnalyzeTests(unittest.TestCase):
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
