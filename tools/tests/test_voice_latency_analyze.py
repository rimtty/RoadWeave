"""Synthetic-clock checks for the external PTT-to-audio measurement gate."""

import math
from pathlib import Path
import sys
import tempfile
import unittest
import wave
from array import array

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from voice_latency_analyze import analyze  # noqa: E402


def make_wav(path: Path, *, missing_second_receiver: bool = False) -> None:
    rate = 8000
    samples = array("h")
    for index in range(rate * 3):
        time = index / rate
        marker = 26000 if 0.20 <= time < 0.27 or 1.20 <= time < 1.27 else 0
        tone1 = (0.32 <= time < 0.52 or 1.34 <= time < 1.54)
        tone2 = (0.35 <= time < 0.55 or not missing_second_receiver and 1.33 <= time < 1.53)
        value = round(8000 * math.sin(2 * math.pi * 1000 * time))
        samples.extend((marker, value if tone1 else 0, value if tone2 else 0))
    with wave.open(str(path), "wb") as recording:
        recording.setnchannels(3)
        recording.setsampwidth(2)
        recording.setframerate(rate)
        recording.writeframes(samples.tobytes())


class VoiceLatencyAnalyzeTests(unittest.TestCase):
    def test_two_receivers_have_independent_latency_and_common_ptt_clock(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "capture.wav"
            make_wav(path)
            report = analyze(path, min_events=2)
        self.assertEqual(report["ptt_count"], 2)
        self.assertTrue(report["pass"])
        self.assertEqual([e["latency_ms"] for e in report["receivers"]["2"]["events"]], [120.0, 140.0])
        self.assertEqual([e["latency_ms"] for e in report["receivers"]["3"]["events"]], [150.0, 130.0])
        self.assertEqual(report["receivers"]["3"]["p95_ms"], 150.0)

    def test_missing_first_audio_is_a_failure_not_removed_from_gate(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "capture.wav"
            make_wav(path, missing_second_receiver=True)
            report = analyze(path, min_events=2)
        self.assertFalse(report["pass"])
        self.assertEqual(report["receivers"]["3"]["missing"], 1)
        self.assertEqual(report["receivers"]["3"]["events"][1]["status"], "missing")

    def test_rejects_duplicate_or_ptt_receiver_channel(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "capture.wav"
            make_wav(path)
            with self.assertRaises(ValueError):
                analyze(path, receiver_channels=[1])
            with self.assertRaises(ValueError):
                analyze(path, receiver_channels=[2, 2])


if __name__ == "__main__":
    unittest.main()
