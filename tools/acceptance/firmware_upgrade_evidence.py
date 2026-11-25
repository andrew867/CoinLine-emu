#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Compare completed, isolated dual-flash snapshots without changing them."""

import argparse
import hashlib
import json
from pathlib import Path

BLOCK_BYTES = 4096
CHIP_BYTES = 524288
IMAGE_BYTES = CHIP_BYTES * 2
BLOCK_COUNT = IMAGE_BYTES // BLOCK_BYTES


def digest(data):
    return hashlib.sha256(data).hexdigest()


def image(path):
    data = path.read_bytes()
    if len(data) != IMAGE_BYTES:
        raise ValueError("A mapped dual-flash image must contain exactly 1 MiB.")
    return data


def persisted(path, seed_sha256):
    raw = path.read_bytes()
    if len(raw) != 64 + IMAGE_BYTES or raw[:64] != seed_sha256.encode("ascii"):
        raise ValueError("Persistent flash length or seed binding does not match the original image.")
    return raw[64:]


def block(data, index):
    return data[index * BLOCK_BYTES:(index + 1) * BLOCK_BYTES]


def summarize(seed, before, after, expected=None, expected_blocks=None):
    for data in (seed, before, after, expected):
        if data is not None and len(data) != IMAGE_BYTES:
            raise ValueError("Every compared image must be exactly 1 MiB.")
    if expected_blocks is not None and expected is None:
        raise ValueError("Expected block indices require an independently supplied target image.")
    requested = None
    if expected is not None:
        indices = range(BLOCK_COUNT) if expected_blocks is None else expected_blocks
        if any(type(index) is not int or not 0 <= index < BLOCK_COUNT for index in indices):
            raise ValueError("Block indices must be integers in the mapped dual-flash range.")
        requested = sorted(set(indices))
        if not requested:
            raise ValueError("Select at least one expected block.")
    changed = [index for index in range(BLOCK_COUNT) if block(before, index) != block(after, index)]
    result = {
        "block_bytes": BLOCK_BYTES,
        "image_bytes": IMAGE_BYTES,
        "seed_sha256": digest(seed),
        "before_sha256": digest(before),
        "after_sha256": digest(after),
        "changed_blocks": changed,
        "chips": [
            {"chip": index, "before_sha256": digest(before[index * CHIP_BYTES:(index + 1) * CHIP_BYTES]),
             "after_sha256": digest(after[index * CHIP_BYTES:(index + 1) * CHIP_BYTES])}
            for index in range(2)
        ],
        "expected": None,
        "service_and_restart_qualified": False,
    }
    if expected is not None:
        mismatches = [index for index in requested if block(expected, index) != block(after, index)]
        result["expected"] = {
            "image_sha256": digest(expected),
            "requested_blocks": requested,
            "mismatched_blocks": mismatches,
            "matches_requested_blocks": not mismatches,
            "changed_outside_requested_blocks": sorted(set(changed) - set(requested)),
            "full_image_matches": expected == after,
        }
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seed", type=Path, required=True, help="original mapped flash input")
    parser.add_argument("--before", type=Path, help="initial saved program_flash, if different from seed")
    parser.add_argument("--after", type=Path, required=True, help="saved program_flash from the completed lab run")
    parser.add_argument("--expected-image", type=Path, help="independently approved mapped target image")
    parser.add_argument("--expected-block", type=int, action="append", help="expected zero-based 4 KiB block; repeat as needed")
    parser.add_argument("--output", type=Path, required=True, help="new JSON evidence report")
    args = parser.parse_args()
    seed = image(args.seed)
    initial = persisted(args.before, digest(seed)) if args.before else seed
    final = persisted(args.after, digest(seed))
    expected = image(args.expected_image) if args.expected_image else None
    report = summarize(seed, initial, final, expected, args.expected_block)
    # Do not overwrite earlier evidence or any input file.
    with args.output.open("x", encoding="utf-8") as output:
        json.dump(report, output, indent=2)
        output.write("\n")
    return 1 if report["expected"] and not report["expected"]["matches_requested_blocks"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
