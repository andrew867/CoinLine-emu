#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Build a deterministic, synthetic one-block target from a registered lab image."""

import argparse
import hashlib
import json
from pathlib import Path

BLOCK_BYTES = 4096
IMAGE_BYTES = 1024 * 1024
DEFAULT_BLOCK = 0x80
MARKER = b"COINLINE-DLA-LAB-1"


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def crc16_xmodem(data):
    crc = 0
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xffff if crc & 0x8000 else (crc << 1) & 0xffff
    return crc


def synthetic_target(seed, block_index=DEFAULT_BLOCK, marker=MARKER):
    if len(seed) != IMAGE_BYTES:
        raise ValueError("The registered mapped image must contain exactly 1 MiB.")
    if type(block_index) is not int or not 0x80 <= block_index < 0xd0:
        raise ValueError("Select an unused main-program block from 0x80 through 0xCF.")
    if not marker or len(marker) > BLOCK_BYTES:
        raise ValueError("The synthetic marker must fit in one 4 KiB block.")
    start = block_index * BLOCK_BYTES
    original = seed[start:start + BLOCK_BYTES]
    if original != bytes([0xff]) * BLOCK_BYTES:
        raise ValueError("The selected block is not erased in the registered source image.")
    target = bytearray(seed)
    target[start:start + len(marker)] = marker
    block = bytes(target[start:start + BLOCK_BYTES])
    return bytes(target), {
        "kind": "synthetic-compatible-image",
        "block_bytes": BLOCK_BYTES,
        "source_sha256": sha256(seed),
        "target_sha256": sha256(target),
        "requested_blocks": [block_index],
        "block_crc16_xmodem": {str(block_index): f"{crc16_xmodem(block):04x}"},
        "marker_sha256": sha256(marker),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seed", type=Path, required=True, help="registered 1 MiB mapped lab image")
    parser.add_argument("--source-sha256", required=True, help="registered digest expected for the source image")
    parser.add_argument("--block", type=lambda value: int(value, 0), default=DEFAULT_BLOCK,
                        help="erased 4 KiB main-program block, default 0x80")
    parser.add_argument("--target", type=Path, required=True, help="new synthetic target image")
    parser.add_argument("--manifest", type=Path, required=True, help="new target registration manifest")
    args = parser.parse_args()
    seed = args.seed.read_bytes()
    if sha256(seed) != args.source_sha256.lower():
        parser.error("--source-sha256 does not match the registered source image")
    target, manifest = synthetic_target(seed, args.block)
    for path in (args.target, args.manifest):
        if path.exists():
            parser.error(f"Refusing to overwrite existing output: {path}")
        path.parent.mkdir(parents=True, exist_ok=True)
    args.target.write_bytes(target)
    with args.manifest.open("x", encoding="utf-8") as output:
        json.dump(manifest, output, indent=2)
        output.write("\n")
    print(json.dumps(manifest, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
