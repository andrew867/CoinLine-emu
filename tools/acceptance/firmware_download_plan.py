#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Describe the 4 KiB delta between two registered mapped firmware images."""

import argparse
import hashlib
import json
from pathlib import Path

BLOCK_BYTES = 4096
IMAGE_BYTES = 1024 * 1024
BLOCK_COUNT = IMAGE_BYTES // BLOCK_BYTES
CRC_METADATA_BLOCK = 191
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


def firmware_applied_target(source, target, requested_blocks,
                            metadata_block=CRC_METADATA_BLOCK):
    if len(source) != IMAGE_BYTES or len(target) != IMAGE_BYTES:
        raise ValueError("Both registered mapped images must contain exactly 1 MiB.")
    active = bytearray(source)
    metadata = metadata_block * BLOCK_BYTES
    for index in requested_blocks:
        if type(index) is not int or not 0 <= index < BLOCK_COUNT or index == metadata_block:
            raise ValueError("Requested block index is outside the transferable mapped-image range.")
        start = index * BLOCK_BYTES
        active[start:start + BLOCK_BYTES] = target[start:start + BLOCK_BYTES]
        entry = metadata + index * 2
        active[entry:entry + 2] = crc16_xmodem(target[start:start + BLOCK_BYTES]).to_bytes(2, "little")
    table_crc = crc16_xmodem(active[metadata:metadata + CRC_TABLE_BYTES])
    table_entry = metadata + CRC_TABLE_CRC_OFFSET
    active[table_entry:table_entry + 2] = table_crc.to_bytes(2, "little")
    return bytes(active)


def registered_download_plan(source, target, retained_blocks=(),
                             metadata_block=CRC_METADATA_BLOCK):
    if len(source) != IMAGE_BYTES or len(target) != IMAGE_BYTES:
        raise ValueError("Both registered mapped images must contain exactly 1 MiB.")
    retained = sorted(set(retained_blocks))
    if any(type(index) is not int or not 0 <= index < BLOCK_COUNT for index in retained):
        raise ValueError("Retained block index is outside the mapped image.")
    changed = [
        index for index in range(BLOCK_COUNT)
        if source[index * BLOCK_BYTES:(index + 1) * BLOCK_BYTES]
        != target[index * BLOCK_BYTES:(index + 1) * BLOCK_BYTES]
    ]
    if not changed:
        raise ValueError("The registered source and target images are identical.")
    requested = [index for index in changed if index != metadata_block and index not in retained]
    if not requested:
        raise ValueError("The registered image delta contains no transferable blocks.")
    active = firmware_applied_target(source, target, requested, metadata_block)
    return {
        "kind": "registered-image-delta",
        "block_bytes": BLOCK_BYTES,
        "image_bytes": IMAGE_BYTES,
        "source_sha256": sha256(source),
        "target_sha256": sha256(target),
        "changed_blocks": changed,
        "requested_blocks": requested,
        "retained_blocks": retained,
        "firmware_metadata_block": metadata_block,
        "expected_active_sha256": sha256(active),
        "block_crc16_xmodem": {
            str(index): f"{crc16_xmodem(target[index * BLOCK_BYTES:(index + 1) * BLOCK_BYTES]):04x}"
            for index in requested
        },
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True, help="registered source mapped image")
    parser.add_argument("--source-sha256", required=True, help="registered digest expected for the source image")
    parser.add_argument("--target", type=Path, required=True, help="registered target mapped image")
    parser.add_argument("--target-sha256", required=True, help="registered digest expected for the target image")
    parser.add_argument("--retained-block", type=int, action="append", default=[],
                        help="active DLA block retained from the source; repeat when required")
    parser.add_argument("--manifest", type=Path, required=True, help="new read-only delta manifest")
    args = parser.parse_args()
    source = args.source.read_bytes()
    target = args.target.read_bytes()
    if sha256(source) != args.source_sha256.lower():
        parser.error("--source-sha256 does not match the registered source image")
    if sha256(target) != args.target_sha256.lower():
        parser.error("--target-sha256 does not match the registered target image")
    if args.manifest.exists():
        parser.error(f"Refusing to overwrite existing output: {args.manifest}")
    plan = registered_download_plan(source, target, args.retained_block)
    args.manifest.parent.mkdir(parents=True, exist_ok=True)
    with args.manifest.open("x", encoding="utf-8") as output:
        json.dump(plan, output, indent=2)
        output.write("\n")
    print(json.dumps(plan, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
