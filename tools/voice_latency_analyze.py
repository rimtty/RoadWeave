#!/usr/bin/env python3
"""Measure PTT-edge to first received test tone from one multichannel WAV clock.

Channel numbers on the command line are one-based. The recording must contain a
PTT marker and the electrical audio outputs of the receiving nodes. No clock
synchronization between RoadWeave devices is needed for this measurement.
"""

from __future__ import annotations

import argparse
from array import array
import hashlib
import json
import math
from pathlib import Path
import sys
import wave


def tone_amplitude(samples: array, channel: int, channels: int, frequency: float, sample_rate: int) -> tuple[float, float]:
    """Return target-tone peak amplitude and its fraction of the window's power."""
    count = len(samples) // channels
    coefficient = 2.0 * math.cos(2.0 * math.pi * frequency / sample_rate)
    previous = previous_previous = squared = 0.0
    for index in range(channel, len(samples), channels):
        sample = samples[index] / 32768.0
        current = sample + coefficient * previous - previous_previous
        previous_previous, previous = previous, current
        squared += sample * sample
    power = previous * previous + previous_previous * previous_previous - coefficient * previous * previous_previous
    amplitude = 2.0 * math.sqrt(max(power, 0.0)) / count
    fraction = (amplitude * amplitude / 2.0) / (squared / count) if squared else 0.0
    return amplitude, min(fraction, 1.0)


def rising_edges(flags: list[bool], hold_windows: int) -> list[int]:
    """Return the first window of each qualifying low-to-high transition."""
    edges = []
    armed = bool(flags) and not flags[0]
    index = 0
    while index < len(flags):
        if not flags[index]:
            armed = True
            index += 1
            continue
        end = index + 1
        while end < len(flags) and flags[end]:
            end += 1
        if armed and end - index >= hold_windows:
            edges.append(index)
        index = end
    return edges


def nearest_rank(values: list[float], quantile: float) -> float | None:
    if not values:
        return None
    ordered = sorted(values)
    return round(ordered[max(0, math.ceil(quantile * len(ordered)) - 1)], 3)


def digest_file(path: Path) -> str:
    checksum = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            checksum.update(block)
    return checksum.hexdigest()


def analyze(
    path: Path, *, ptt_channel: int = 1, receiver_channels: list[int] | None = None,
    window_ms: float = 5.0, trigger_level: float = 0.2, tone_hz: float = 1000.0,
    tone_level: float = 0.03, min_tone_fraction: float = 0.6,
    max_latency_ms: float = 1000.0, deadline_ms: float = 150.0,
    min_events: int = 30,
) -> dict:
    if window_ms <= 0 or max_latency_ms <= 0 or deadline_ms <= 0 or min_events < 1:
        raise ValueError("window, latency, deadline, and min-events must be positive")
    if not 0 < trigger_level < 1 or not 0 < tone_level < 1 or not 0 <= min_tone_fraction <= 1:
        raise ValueError("levels must be within (0,1), and tone fraction within [0,1]")
    with wave.open(str(path), "rb") as recording:
        if recording.getcomptype() != "NONE" or recording.getsampwidth() != 2:
            raise ValueError("input must be uncompressed 16-bit PCM WAV")
        channels = recording.getnchannels()
        sample_rate = recording.getframerate()
        if not 0 < tone_hz < sample_rate / 2:
            raise ValueError("tone frequency must be below Nyquist")
        if receiver_channels is None:
            receiver_channels = [number for number in range(1, channels + 1) if number != ptt_channel]
        if not 1 <= ptt_channel <= channels or not receiver_channels:
            raise ValueError("PTT and at least one receiver channel must exist")
        if len(set(receiver_channels)) != len(receiver_channels) or ptt_channel in receiver_channels:
            raise ValueError("receiver channels must be unique and different from PTT")
        if any(not 1 <= number <= channels for number in receiver_channels):
            raise ValueError("receiver channel outside WAV channel count")
        window_samples = max(1, round(sample_rate * window_ms / 1000.0))
        actual_window_ms = 1000.0 * window_samples / sample_rate
        marker_flags: list[bool] = []
        tone_flags = {number: [] for number in receiver_channels}
        while recording.tell() + window_samples <= recording.getnframes():
            frame = array("h")
            frame.frombytes(recording.readframes(window_samples))
            if sys.byteorder != "little":
                frame.byteswap()
            count = len(frame) // channels
            marker = sum(frame[ptt_channel - 1::channels]) / (32768.0 * count)
            marker_flags.append(marker >= trigger_level)
            for number in receiver_channels:
                amplitude, fraction = tone_amplitude(frame, number - 1, channels, tone_hz, sample_rate)
                tone_flags[number].append(amplitude >= tone_level and fraction >= min_tone_fraction)

    hold = max(2, math.ceil(10.0 / actual_window_ms))
    marker_edges = rising_edges(marker_flags, hold)
    if not marker_edges:
        raise ValueError("no PTT edges found; record a low marker before the first press")
    result = {
        "source": str(path), "sha256": digest_file(path), "sample_rate_hz": sample_rate,
        "channels": channels, "window_ms": round(actual_window_ms, 6),
        "resolution_ms": round(actual_window_ms, 6), "ptt_channel": ptt_channel,
        "receiver_channels": receiver_channels, "tone_hz": tone_hz,
        "deadline_ms": deadline_ms, "max_latency_ms": max_latency_ms,
        "min_events": min_events, "ptt_count": len(marker_edges), "receivers": {},
    }
    for number in receiver_channels:
        onsets = rising_edges(tone_flags[number], hold)
        events = []
        for ordinal, marker_edge in enumerate(marker_edges, start=1):
            next_marker = marker_edges[ordinal] if ordinal < len(marker_edges) else len(marker_flags)
            deadline_window = min(next_marker, marker_edge + math.ceil(max_latency_ms / actual_window_ms))
            if tone_flags[number][marker_edge]:
                events.append({"ptt_index": ordinal, "ptt_ms": round(marker_edge * actual_window_ms, 3),
                               "status": "tone_already_active", "latency_ms": None})
                continue
            onset = next((edge for edge in onsets if marker_edge <= edge < deadline_window), None)
            events.append({"ptt_index": ordinal, "ptt_ms": round(marker_edge * actual_window_ms, 3),
                           "status": "ok" if onset is not None else "missing",
                           "latency_ms": round((onset - marker_edge) * actual_window_ms, 3) if onset is not None else None})
        values = [event["latency_ms"] for event in events if event["status"] == "ok"]
        missing = sum(event["status"] == "missing" for event in events)
        invalid = sum(event["status"] == "tone_already_active" for event in events)
        p95 = nearest_rank(values, 0.95)
        result["receivers"][str(number)] = {
            "events": events, "received": len(values), "missing": missing, "invalid": invalid,
            "p50_ms": nearest_rank(values, 0.50), "p95_ms": p95,
            "p99_ms": nearest_rank(values, 0.99), "max_ms": max(values) if values else None,
            "pass": len(events) >= min_events and missing == 0 and invalid == 0 and p95 is not None and p95 <= deadline_ms,
        }
    result["pass"] = all(row["pass"] for row in result["receivers"].values())
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("wav", type=Path)
    parser.add_argument("--ptt-channel", type=int, default=1, help="one-based WAV channel number")
    parser.add_argument("--receiver-channels", help="comma-separated one-based channel numbers; default: all except PTT")
    parser.add_argument("--window-ms", type=float, default=5.0)
    parser.add_argument("--trigger-level", type=float, default=0.2)
    parser.add_argument("--tone-hz", type=float, default=1000.0)
    parser.add_argument("--tone-level", type=float, default=0.03)
    parser.add_argument("--min-tone-fraction", type=float, default=0.6)
    parser.add_argument("--max-latency-ms", type=float, default=1000.0)
    parser.add_argument("--deadline-ms", type=float, default=150.0)
    parser.add_argument("--min-events", type=int, default=30)
    parser.add_argument("--output", type=Path, help="write JSON report here as well as stdout")
    arguments = parser.parse_args()
    try:
        receivers = [int(item) for item in arguments.receiver_channels.split(",")] if arguments.receiver_channels else None
        report = analyze(arguments.wav, ptt_channel=arguments.ptt_channel, receiver_channels=receivers,
                         window_ms=arguments.window_ms, trigger_level=arguments.trigger_level,
                         tone_hz=arguments.tone_hz, tone_level=arguments.tone_level,
                         min_tone_fraction=arguments.min_tone_fraction, max_latency_ms=arguments.max_latency_ms,
                         deadline_ms=arguments.deadline_ms, min_events=arguments.min_events)
    except (OSError, EOFError, ValueError, wave.Error) as error:
        parser.error(str(error))
    encoded = json.dumps(report, ensure_ascii=False, indent=2)
    if arguments.output:
        arguments.output.parent.mkdir(parents=True, exist_ok=True)
        arguments.output.write_text(encoded + "\n", encoding="utf-8")
    print(encoded)
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
