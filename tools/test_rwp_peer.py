"""Host integration checks: python -m unittest discover -s tools -p test_rwp_peer.py."""
import socket
import subprocess
import sys
import tempfile
import queue
import threading
import unittest
import wave
from pathlib import Path

import rwp_peer as peer


class PeerIntegrationTests(unittest.TestCase):
    def run_peer(self, mode, *extra):
        # Ask the OS for an unused local UDP port before launching the actual CLI.
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
            probe.bind(('127.0.0.1', 0))
            port = probe.getsockname()[1]
        process = subprocess.Popen(
            [sys.executable, '-u', str(Path(peer.__file__)), mode, *map(str, extra),
             '--bind', '127.0.0.1', '--port', str(port), '--duration', '1'],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
        )
        self.addCleanup(self.stop_peer, process)
        ready = queue.Queue()
        reader = threading.Thread(target=lambda: ready.put(process.stdout.readline()), daemon=True)
        reader.start()
        line = ready.get(timeout=5)
        reader.join(timeout=1)
        self.assertIn('listening on 127.0.0.1', line)
        return process, port

    @staticmethod
    def stop_peer(process):
        if process.poll() is None:
            process.kill()
        process.communicate(timeout=5)

    def exchange(self, port, packet):
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as client:
            client.settimeout(0.1)
            for _ in range(8):
                client.sendto(packet, ('127.0.0.1', port))
                try:
                    received, _ = client.recvfrom(2048)
                    self.assertEqual(received, packet)
                    return
                except socket.timeout:
                    continue
        self.fail('Peer did not echo the datagram')

    def test_codec_and_header_vectors(self):
        peer.run_selftest()

    def test_adpcm_and_opus_datagrams_echo_unchanged(self):
        process, port = self.run_peer('echo')
        for codec in (0, 1):
            packet = peer.build(peer.T_VOICE, codec, peer.FLAG_START, 1, 0x101, 7,
                                codec, 12345, b'opaque codec payload')
            self.exchange(port, packet)
        stdout, stderr = process.communicate(timeout=5)
        self.assertEqual(process.returncode, 0, stderr)

    def test_duration_exits_without_packets(self):
        for duration in ('0', '-1', 'nan', 'inf'):
            with self.subTest(duration=duration):
                result = subprocess.run(
                    [sys.executable, str(Path(peer.__file__)), 'echo', '--duration', duration],
                    capture_output=True, text=True, timeout=5,
                )
                self.assertEqual(result.returncode, 2, result.stderr)
        process, _ = self.run_peer('echo')
        _, stderr = process.communicate(timeout=5)
        self.assertEqual(process.returncode, 0, stderr)

    def test_record_finalizes_wav(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'record.wav'
            process, port = self.run_peer('record', path)
            block = peer.adpcm_encode_block(peer.Adpcm(), [0] * 320)
            self.exchange(port, peer.build(peer.T_VOICE, 0, peer.FLAG_START, 1,
                                           0x101, 7, 0, 0, block))
            _, stderr = process.communicate(timeout=5)
            self.assertEqual(process.returncode, 0, stderr)
            with wave.open(str(path), 'rb') as recorded:
                self.assertEqual(recorded.getframerate(), 16000)
                self.assertEqual(recorded.getnchannels(), 1)
                self.assertEqual(recorded.getsampwidth(), 2)
                self.assertGreaterEqual(recorded.getnframes(), 320)


if __name__ == '__main__':
    unittest.main()
