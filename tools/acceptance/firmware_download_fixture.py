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
CRC_METADATA_BLOCK = 0xBF
CRC_TABLE_BYTES = 0x200
CRC_TABLE_CRC_OFFSET = 0x200


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def crc16_xmodem(data):
    crc = 0
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xffff if crc & 0x8000 else (crc << 1) & 0xffff
    return crc


def expected_active_target(target, requested_blocks):
    """Apply the firmware-maintained block and checksum-table metadata."""
    if len(target) != IMAGE_BYTES:
        raise ValueError("The target mapped image must contain exactly 1 MiB.")
    active = bytearray(target)
    metadata = CRC_METADATA_BLOCK * BLOCK_BYTES
    for block_index in requested_blocks:
        if type(block_index) is not int or not 0 <= block_index < IMAGE_BYTES // BLOCK_BYTES:
            raise ValueError("Requested block index is outside the mapped image.")
        start = block_index * BLOCK_BYTES
        block_crc = crc16_xmodem(target[start:start + BLOCK_BYTES])
        entry = metadata + block_index * 2
        active[entry:entry + 2] = block_crc.to_bytes(2, "little")
    table_crc = crc16_xmodem(active[metadata:metadata + CRC_TABLE_BYTES])
    table_entry = metadata + CRC_TABLE_CRC_OFFSET
    active[table_entry:table_entry + 2] = table_crc.to_bytes(2, "little")
    return bytes(active)


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
    active = expected_active_target(target, [block_index])
    return bytes(target), {
        "kind": "synthetic-compatible-image",
        "block_bytes": BLOCK_BYTES,
        "source_sha256": sha256(seed),
        "target_sha256": sha256(target),
        "expected_active_sha256": sha256(active),
        "requested_blocks": [block_index],
        "firmware_metadata_blocks": [CRC_METADATA_BLOCK],
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
    parser.add_argument("--active-target", type=Path,
                        help="expected active image after firmware checksum metadata is applied")
    parser.add_argument("--manifest", type=Path, required=True, help="new target registration manifest")
    args = parser.parse_args()
    seed = args.seed.read_bytes()
    if sha256(seed) != args.source_sha256.lower():
        parser.error("--source-sha256 does not match the registered source image")
    target, manifest = synthetic_target(seed, args.block)
    outputs = [args.target, args.manifest]
    if args.active_target:
        outputs.append(args.active_target)
    for path in outputs:
        if path.exists():
            parser.error(f"Refusing to overwrite existing output: {path}")
        path.parent.mkdir(parents=True, exist_ok=True)
    args.target.write_bytes(target)
    if args.active_target:
        args.active_target.write_bytes(expected_active_target(target, manifest["requested_blocks"]))
    with args.manifest.open("x", encoding="utf-8") as output:
        json.dump(manifest, output, indent=2)
        output.write("\n")
    print(json.dumps(manifest, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
