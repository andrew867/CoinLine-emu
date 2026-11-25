#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Generate a deterministic, non-network 8 kHz CAS/CONNECT line stimulus."""

import argparse
import hashlib
import json
import math
from pathlib import Path
import struct


RATE = 8000
CONNECT = bytes.fromhex("84 05 01 8E 00 86 00 62")


def stimulus(gap_ms=320, connect_pcm=None):
    if not 0 <= gap_ms <= 2000:
        raise ValueError("gap must be between zero and 2000 ms")
    samples = [0] * RATE
    cas_start = len(samples)
    samples.extend(round(2600 * (math.sin(math.tau * 2130 * i / RATE) +
                                 math.sin(math.tau * 2750 * i / RATE))) for i in range(680))
    cas_end = len(samples)
    samples.extend([0] * (gap_ms * 8))
    fsk_start = len(samples)
    if connect_pcm is None:
        symbols = [1] * 180
        for octet in CONNECT:
            symbols.extend([0, *((octet >> bit) & 1 for bit in range(8)), 1])
        symbols.extend([1] * 12)
        phase = 0.0
        for i in range((len(symbols) * RATE + 1199) // 1200):
            phase += math.tau * (1200 if symbols[i * 1200 // RATE] else 2200) / RATE
            samples.append(round(3800 * math.sin(phase)))
    else:
        if not connect_pcm or len(connect_pcm) % 2 or len(connect_pcm) > 4 * RATE:
            raise ValueError("captured CONNECT must contain up to two seconds of signed 8 kHz PCM")
        samples.extend(value[0] for value in struct.iter_unpack("<h", connect_pcm))
    fsk_end = len(samples)
    samples.extend([0] * (2 * RATE))
    raw = struct.pack(f"<{len(samples)}h", *samples)
    return raw, {"sample_rate": RATE, "samples": len(samples), "cas_start": cas_start,
                 "cas_end": cas_end, "fsk_segment_start": fsk_start,
                 "fsk_segment_end": fsk_end, "sha256": hashlib.sha256(raw).hexdigest(),
                 "connect_source": "captured PCM" if connect_pcm is not None else "synthetic Bell 202"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--gap-ms", type=int, default=320)
    parser.add_argument("--connect-pcm", type=Path, help="optional local captured CONNECT segment, including any silence")
    args = parser.parse_args()
    raw, report = stimulus(args.gap_ms, args.connect_pcm.read_bytes() if args.connect_pcm else None)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("xb") as out:
        out.write(raw)
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
