#!/usr/bin/env python3
"""RoadWeave RWP/0.1 peer for Windows/macOS/Linux (stdlib only).

Modes:
  echo                 echo every RWP datagram back to its sender (RTT / mouth-to-ear on the node)
  record OUT.wav       decode incoming IMA-ADPCM voice and write 16 kHz mono WAV (also echoes unless --no-echo)
  send IN.wav          stream a 16 kHz mono 16-bit WAV to the node as RWP voice at 20 ms cadence
  selftest             verify the Python ADPCM codec against the C test vector

Examples:
  python3 tools/rwp_peer.py echo
  python3 tools/rwp_peer.py record mic.wav
  python3 tools/rwp_peer.py send hello16k.wav --to 192.168.1.42
"""
import argparse, math, socket, struct, sys, time, wave

MAGIC = 0x5257
VERSION = 1
HEADER_LEN = 36
HDR = struct.Struct(">HBBBHHIIBIIIIH")   # magic ver type codec flags header_len group sender ttype tid stream seq ctime plen
T_VOICE = 1
CODEC_ADPCM = 0
FLAG_START = 1 << 1

STEP = [7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97,
        107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
        876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428,
        4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350,
        22385, 24623, 27086, 29794, 32767]
INDEX = [-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8]


class Adpcm:
    def __init__(self):
        self.pred = 0
        self.idx = 0

    def encode(self, sample):
        step = STEP[self.idx]
        diff = sample - self.pred
        code = 0
        if diff < 0:
            code = 8
            diff = -diff
        delta = step >> 3
        if diff >= step:
            code |= 4; diff -= step; delta += step
        step >>= 1
        if diff >= step:
            code |= 2; diff -= step; delta += step
        step >>= 1
        if diff >= step:
            code |= 1; delta += step
        pred = self.pred - delta if code & 8 else self.pred + delta
        self.pred = max(-32768, min(32767, pred))
        self.idx = max(0, min(88, self.idx + INDEX[code]))
        return code

    def decode(self, nib):
        nib &= 0xF
        step = STEP[self.idx]
        delta = step >> 3
        if nib & 4: delta += step
        if nib & 2: delta += step >> 1
        if nib & 1: delta += step >> 2
        pred = self.pred - delta if nib & 8 else self.pred + delta
        self.pred = max(-32768, min(32767, pred))
        self.idx = max(0, min(88, self.idx + INDEX[nib]))
        return self.pred


def adpcm_encode_block(state, pcm):
    out = bytearray(struct.pack(">hBB", state.pred, state.idx, 0))
    for i in range(0, len(pcm), 2):
        lo = state.encode(pcm[i]); hi = state.encode(pcm[i + 1])
        out.append(lo | (hi << 4))
    return bytes(out)


def adpcm_decode_block(blk):
    pred, idx, _ = struct.unpack(">hBB", blk[:4])
    st = Adpcm(); st.pred, st.idx = pred, idx
    pcm = []
    for b in blk[4:]:
        pcm.append(st.decode(b & 0xF)); pcm.append(st.decode(b >> 4))
    return pcm


def parse(dgram):
    if len(dgram) < HEADER_LEN:
        return None
    f = HDR.unpack_from(dgram, 0)
    magic, ver, typ, codec, flags, hlen, group, sender, ttype, tid, stream, seq, ctime, plen = f
    if magic != MAGIC or ver != VERSION or hlen < HEADER_LEN or hlen + plen > len(dgram):
        return None
    return dict(type=typ, codec=codec, flags=flags, group=group, sender=sender, stream=stream, seq=seq,
                ctime=ctime, payload=dgram[hlen:hlen + plen])


def build(typ, codec, flags, group, sender, stream, seq, ctime, payload):
    return HDR.pack(MAGIC, VERSION, typ, codec, flags, HEADER_LEN, group, sender, 0, 0, stream, seq, ctime & 0xFFFFFFFF, len(payload)) + payload


class Stats:
    def __init__(self):
        self.rx = 0; self.bad = 0; self.lost = 0; self.reorder = 0; self.last = {}; self.t0 = time.time(); self.last_print = self.t0
        self.arrivals = []

    def on(self, p, t):
        self.rx += 1
        key = (p["sender"], p["stream"])
        if key in self.last:
            exp = (self.last[key] + 1) & 0xFFFFFFFF
            if p["seq"] != exp:
                d = (p["seq"] - exp) & 0xFFFFFFFF
                if d < 0x80000000: self.lost += d
                else: self.reorder += 1
        self.last[key] = p["seq"]
        self.arrivals.append(t)
        if t - self.last_print >= 1.0:
            self.last_print = t
            ia = [b - a for a, b in zip(self.arrivals, self.arrivals[1:])]
            self.arrivals = self.arrivals[-1:]
            jitter = (max(ia) - min(ia)) * 1000 if len(ia) > 1 else 0
            print(f"rx {self.rx} lost {self.lost} reorder {self.reorder} bad {self.bad} | inter-arrival jitter {jitter:5.1f} ms | streams {len(self.last)}", flush=True)


def run_echo(args, record=None):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((args.bind, args.port))
    sock.settimeout(0.2)
    deadline = time.monotonic() + args.duration if args.duration else None
    print(f"listening on {args.bind}:udp/{args.port}  (echo={'off' if args.no_echo else 'on'}{', recording ' + record if record else ''})", flush=True)
    st = Stats(); wav = None; pcm_total = 0
    if record:
        wav = wave.open(record, "wb"); wav.setnchannels(1); wav.setsampwidth(2); wav.setframerate(16000)
    try:
        while deadline is None or time.monotonic() < deadline:
            try:
                d, addr = sock.recvfrom(2048)
            except socket.timeout:
                continue
            t = time.time()
            p = parse(d)
            if p is None:
                st.bad += 1; continue
            st.on(p, t)
            if not args.no_echo:
                sock.sendto(d, addr)
            if wav and p["type"] == T_VOICE and p["codec"] == CODEC_ADPCM:
                pcm = adpcm_decode_block(p["payload"])
                wav.writeframes(struct.pack("<%dh" % len(pcm), *pcm)); pcm_total += len(pcm)
    except KeyboardInterrupt:
        pass
    finally:
        sock.close()
        if wav:
            wav.close(); print(f"wrote {record}: {pcm_total / 16000:.1f} s")


def run_send(args):
    w = wave.open(args.file, "rb")
    assert w.getnchannels() == 1 and w.getsampwidth() == 2 and w.getframerate() == 16000, "need 16 kHz mono 16-bit WAV"
    frames = w.readframes(w.getnframes()); w.close()
    pcm = list(struct.unpack("<%dh" % (len(frames) // 2), frames))
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    st = Adpcm(); stream = int(time.time()) & 0xFFFFFFFF; seq = 0; t0 = time.time()
    n_blocks = len(pcm) // 320
    print(f"sending {n_blocks} x 20 ms to {args.to}:{args.port}")
    for b in range(n_blocks):
        blk = adpcm_encode_block(st, pcm[b * 320:(b + 1) * 320])
        ctime = int((time.time() - t0) * 1000)
        sock.sendto(build(T_VOICE, CODEC_ADPCM, FLAG_START if seq == 0 else 0, args.group, args.sender, stream, seq, ctime, blk), (args.to, args.port))
        seq += 1
        target = t0 + (b + 1) * 0.02
        while time.time() < target: time.sleep(0.001)
    print("done")


def run_selftest():
    # Vector printed by firmware/components/audio_pipeline/test_host/test_adpcm.c
    st = Adpcm()
    blk = adpcm_encode_block(st, [0, 100, 200, 300, 400, 500, 600, 700])
    assert blk.hex() == "0000000070776711", blk.hex()
    assert adpcm_decode_block(blk) == [0, 11, 41, 104, 240, 494, 597, 691]
    # header roundtrip
    d = build(T_VOICE, CODEC_ADPCM, FLAG_START, 1, 0x101, 7, 9, 12345, blk)
    p = parse(d)
    assert p and p["seq"] == 9 and p["stream"] == 7 and p["payload"] == blk and p["sender"] == 0x101
    assert parse(d[:HEADER_LEN - 1]) is None and parse(b"\x00" + d[1:]) is None
    print("selftest OK")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("mode", choices=["echo", "record", "send", "selftest"])
    ap.add_argument("file", nargs="?")
    ap.add_argument("--port", type=int, default=5004)
    ap.add_argument("--to", default="192.168.1.42", help="node IP for send mode")
    ap.add_argument("--no-echo", action="store_true")
    ap.add_argument("--bind", default="0.0.0.0", help="local IPv4 to listen on (echo/record)")
    ap.add_argument("--duration", type=float, help="stop echo/record after this many seconds, even without packets")
    ap.add_argument("--group", type=lambda x: int(x, 0), default=1)
    ap.add_argument("--sender", type=lambda x: int(x, 0), default=0x201)
    args = ap.parse_args()
    if args.duration is not None and (not math.isfinite(args.duration) or args.duration <= 0 or args.mode not in ("echo", "record")):
        ap.error("--duration must be finite, positive and used with echo or record")
    if args.mode == "selftest": run_selftest()
    elif args.mode == "echo": run_echo(args)
    elif args.mode == "record":
        if not args.file: sys.exit("record needs an output WAV path")
        run_echo(args, record=args.file)
    elif args.mode == "send":
        if not args.file: sys.exit("send needs an input WAV path")
        run_send(args)


if __name__ == "__main__":
    main()
