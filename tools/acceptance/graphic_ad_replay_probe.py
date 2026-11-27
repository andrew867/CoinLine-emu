#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Compare native VFD snapshots against recorded bus writes after hook cycles.

Run graphic_session.py with --trace and named baseline/off/on snapshots.
This is an observer only: no live terminal, memory writes or display injection.
"""
import argparse
import json
from pathlib import Path

from PIL import Image

WIDTH, HEIGHT, STRIDE, PAGE_BYTES = 126, 88, 128, 1408


def replay(run: Path, cpu_hz: int, fps: int, graphic_y: int):
    events = [(int(frame), detail) for frame, action, detail in
              (line.split("\t", 2) for line in (run / "events.tsv").read_text().splitlines())
              if action == "snapshot"]
    pages = [bytearray(PAGE_BYTES) for _ in range(4)]
    control, released = 3, False
    counts = {"pixel_writes": 0, "graphic_nonzero_writes": 0, "clear_strobes": 0}
    results = []

    def snapshot(frame, name):
        with Image.open(run / f"{name}.png") as source:
            image = source.convert("RGB")
            scale = min(image.width // WIDTH, image.height // HEIGHT)
            if scale < 1:
                raise ValueError("Native graphical VFD snapshot required")
            left = (image.width - WIDTH * scale) // 2
            top = (image.height - HEIGHT * scale) // 2
            screen = [image.getpixel((left+x*scale+scale//2, top+y*scale+scale//2))[1] > 20
                      for y in range(HEIGHT) for x in range(WIDTH)]
        visible = (control >> 2) & 3
        # The qualified 11-line profile has an active-low all-dots lamp bit.
        expected = [bool(released and control & 0x10 and
                         (not control & 0x20 or pages[visible][(10-y//8)*STRIDE+x] & (1 << (y%8))))
                    for y in range(HEIGHT) for x in range(WIDTH)]
        offset = graphic_y * WIDTH
        results.append({"frame": frame, "snapshot": name, "visible_page": visible,
                        "control": control, "screen_graphics_dots": sum(screen[offset:]),
                        "bus_graphics_dots": sum(expected[offset:]),
                        "mismatched_dots": sum(a != b for a, b in zip(screen, expected)), **counts})
        for key in counts:
            counts[key] = 0

    index, last_cycle = 0, -1
    with (run / "bus.jsonl").open() as trace:
        for line in trace:
            row = json.loads(line)
            cycle = row["cycle"]
            if cycle < last_cycle:
                raise ValueError("Trace cycle discontinuity; replay a single uninterrupted run")
            last_cycle = cycle
            while index < len(events) and cycle * fps > events[index][0] * cpu_hz:
                snapshot(*events[index])
                index += 1
            if row["direction"] != "w":
                continue
            port, value, write_page = row["port"], row["value"], row["write_page"]
            if 0x1280 <= port <= 0x17ff or 0x1a80 <= port <= 0x1fff:
                offset = port - (0x1a80 if port >= 0x1a80 else 0x1280)
                if released and control & 2:
                    pages[write_page][offset] = value
                    counts["pixel_writes"] += 1
                    if offset < PAGE_BYTES - ((graphic_y + 7)//8)*STRIDE and value:
                        counts["graphic_nonzero_writes"] += 1
            elif port & 0xff == 0xf0 and value == 0:
                released = True
            elif port & 0xff == 0x80:
                if released and control & 1 and not value & 1:
                    pages[write_page] = bytearray(PAGE_BYTES)
                    counts["clear_strobes"] += 1
                control = value
    while index < len(events):
        snapshot(*events[index])
        index += 1
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run", type=Path, required=True, help="completed native traced graphical session")
    parser.add_argument("--cpu-hz", type=int, default=12288000)
    parser.add_argument("--fps", type=int, default=60)
    parser.add_argument("--graphic-y", type=int, default=16, help="first VFD pixel row belonging to ads")
    parser.add_argument("--checkpoint-suffix", default="14s", help="evaluate settled on-hook snapshots with this suffix")
    args = parser.parse_args()
    if args.cpu_hz <= 0 or args.fps <= 0 or not 0 <= args.graphic_y < HEIGHT:
        parser.error("Invalid replay geometry or clock")
    rows = replay(args.run, args.cpu_hz, args.fps, args.graphic_y)
    baseline = next((r for r in rows if r["snapshot"] == "baseline"), None)
    settled = [r for r in rows if r["snapshot"].startswith("on-") and
               r["snapshot"].endswith("-" + args.checkpoint_suffix)]
    if baseline is None or not baseline["bus_graphics_dots"] or not settled:
        raise ValueError("A nonblank baseline and settled on-hook checkpoints are required")
    blank = [r["snapshot"] for r in settled if not r["bus_graphics_dots"]]
    mismatches = sum(r["mismatched_dots"] for r in rows)
    result = {"snapshots": len(rows), "mismatched_dots": mismatches,
              "settled_checkpoints": len(settled), "blank_bus_checkpoints": blank,
              "passed": not mismatches and not blank, "observations": rows}
    (args.run / "graphic-ad-replay-result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(f"{len(rows)} snapshots, {mismatches} dot mismatches, blank bus checkpoints: {blank}")
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
