#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Match native VFD captures against the bitmap screens in local graphical records."""
import argparse
import hashlib
import json
from pathlib import Path
import sys

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/vfd"))
from bmp2vfd import PackedGraphic, unpack_graphic


def row_bits(row):
    return sum(value << x for x, value in enumerate(row))


def bitmap_screens(tables):
    for number in (131, 132, 133):
        data = (tables / f"{number}.bin").read_bytes()
        if len(data) != 9224 or data[0] != number:
            raise ValueError(f"Invalid MT{number} graphical record")
        for index in range(8):
            at = 8 + 1152 * index
            if data[at] != ord("B"):
                continue
            payload = data[at + 1:at + 1045]
            graphic = unpack_graphic(PackedGraphic(payload, 112, 112, 9, 36))
            rows = tuple(row_bits(row) for row in graphic.rows)
            if any(rows):
                yield number, index, hashlib.sha256(payload).hexdigest(), rows


def visible_rows(path):
    with Image.open(path) as source:
        image = source.convert("RGB")
        if image.size != (1280, 900):
            raise ValueError(f"Expected native 1280 by 900 VFD snapshot: {path.name}")
        rows = []
        for y in range(88):
            row = []
            for x in range(126):
                red, green, blue = image.getpixel((15 + x * 10, 15 + y * 10))
                row.append(int(green > 20 and green > red and blue > 10))
            rows.append(row_bits(row))
        return rows


def match(rows, expected):
    mask = (1 << 112) - 1
    for y in range(17):
        for x in range(15):
            if all(((rows[y + i] >> x) & mask) == value for i, value in enumerate(expected)):
                return x, y
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tables", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("snapshots", nargs="+", type=Path)
    args = parser.parse_args()
    screens = list(bitmap_screens(args.tables))
    matches = []
    for path in args.snapshots:
        rows = visible_rows(path)
        for number, index, digest, expected in screens:
            origin = match(rows, expected)
            if origin is not None:
                matches.append({"snapshot": path.name, "table": number, "screen": index + 1,
                                "payload_sha256": digest, "origin": list(origin)})
    unique = len({(item["table"], item["screen"]) for item in matches})
    args.output.write_text(json.dumps({"bitmap_screens": len(screens), "captures": len(args.snapshots),
                                       "unique_matches": unique, "matches": matches}, indent=2) + "\n")
    print(f"{unique} distinct graphical record screens matched in {len(args.snapshots)} native captures")
    return 0 if matches else 1


if __name__ == "__main__":
    raise SystemExit(main())
