#!/usr/bin/env python3
"""Generate millennium .lay bounds from the color reference artwork.

This script extracts rectangle bounds from
`artwork/millennium-terminal-front-button-areas.png` and rewrites:
- `src/mame/layout/millennium.lay`
- `artwork/millennium.lay`

The reference image is expected to keep the same color coding used by the
project's alignment workflow.
"""

from __future__ import annotations

import argparse
from pathlib import Path
from typing import Iterable

from PIL import Image


RGB = tuple[int, int, int]
Box = tuple[int, int, int, int]


def find_components(pixels: list[list[RGB]], color: RGB, min_pixels: int = 100) -> list[Box]:
    height = len(pixels)
    width = len(pixels[0]) if height else 0
    seen = [[False] * width for _ in range(height)]
    out: list[Box] = []

    for y in range(height):
        for x in range(width):
            if seen[y][x] or pixels[y][x] != color:
                continue
            stack = [(x, y)]
            seen[y][x] = True
            min_x = max_x = x
            min_y = max_y = y
            count = 0
            while stack:
                cx, cy = stack.pop()
                count += 1
                if cx < min_x:
                    min_x = cx
                if cx > max_x:
                    max_x = cx
                if cy < min_y:
                    min_y = cy
                if cy > max_y:
                    max_y = cy
                for nx, ny in ((cx + 1, cy), (cx - 1, cy), (cx, cy + 1), (cx, cy - 1)):
                    if 0 <= nx < width and 0 <= ny < height and not seen[ny][nx] and pixels[ny][nx] == color:
                        seen[ny][nx] = True
                        stack.append((nx, ny))
            if count >= min_pixels:
                out.append((min_x, min_y, max_x, max_y))

    out.sort(key=lambda b: (b[1], b[0]))
    return out


def to_bounds(box: Box) -> str:
    return f'<bounds left="{box[0]}" top="{box[1]}" right="{box[2]}" bottom="{box[3]}" />'


def pop_single(boxes: list[Box], label: str) -> Box:
    if len(boxes) != 1:
        raise RuntimeError(f"Expected one component for {label}, got {len(boxes)}")
    return boxes[0]


def normalize_numeric_keypad_order(num_boxes: list[Box]) -> list[Box]:
    """Return keypad boxes in logical order 1..9,*0# despite perspective y jitter."""
    if len(num_boxes) != 12:
        raise RuntimeError(f"Expected 12 numeric boxes, found {len(num_boxes)}")
    by_scan = sorted(num_boxes, key=lambda b: (b[1], b[0]))
    ordered: list[Box] = []
    for row in range(4):
        chunk = by_scan[row * 3 : (row + 1) * 3]
        if len(chunk) != 3:
            raise RuntimeError(f"Expected 3 boxes in row {row}, found {len(chunk)}")
        ordered.extend(sorted(chunk, key=lambda b: b[0]))
    return ordered


def render_layout(image_w: int, image_h: int, data: dict[str, Iterable[Box] | Box]) -> str:
    nums: list[Box] = list(data["num_boxes"])  # type: ignore[arg-type]
    quick = sorted(data["quick_boxes"], key=lambda box: box[0])  # type: ignore[arg-type]
    vfd_box: Box = data["vfd_box"]  # type: ignore[assignment]
    down_box: Box = data["down_box"]  # type: ignore[assignment]
    up_box: Box = data["up_box"]  # type: ignore[assignment]
    lang_box: Box = data["lang_box"]  # type: ignore[assignment]
    newcall_box: Box = data["newcall_box"]  # type: ignore[assignment]

    # Keep existing approximations for regions not included in the color map.
    handset_box: Box = (100, 590, 293, 1220)
    handset_button_box: Box = (162, 1376, 442, 1434)
    service_lock_box: Box = (337, 299, 379, 341)
    coin_slot_box: Box = (272, 22, 348, 121)
    card_reader_box: Box = (317, 945, 451, 1065)

    def el(ref: str, tag: str, mask: str, box: Box) -> str:
        return (
            f'\t\t<element ref="{ref}" inputtag="{tag}" inputmask="{mask}">\n'
            f"\t\t\t{to_bounds(box)}\n"
            f"\t\t</element>"
        )

    num_masks = [
        "0x00000001",
        "0x00000002",
        "0x00000004",
        "0x00000008",
        "0x00000010",
        "0x00000020",
        "0x00000040",
        "0x00000080",
        "0x00000100",
        "0x00000200",
        "0x00000400",
        "0x00000800",
    ]
    number_elements = "\n".join(el("hit_num", "KEYMATRIX", m, b) for m, b in zip(num_masks, nums, strict=True))
    quick_elements = "\n".join(
        el("hit_quick", "KEYMATRIX", f"0x{1 << (20 + position):08X}", box)
        for position, box in enumerate(quick)
    )
    indicators = (
        ("housing", "Housing", "Closed", "Open", 0x02),
        ("lock", "Lock", "Locked", "Unlocked", 0x01),
        ("vault", "Coin vault", "Closed", "Open", 0x04),
    )
    indicator_elements = "\n".join(
        f'''\t<element name="indicator_{name}" defstate="0">
\t\t<text string="{label}" align="1">
\t\t\t<bounds x="0.03" y="0.12" width="0.39" height="0.76" />
\t\t\t<color red="0.85" green="0.89" blue="0.94" />
\t\t</text>
\t\t<disk state="0">
\t\t\t<bounds x="0.46" y="0.22" width="0.09" height="0.56" />
\t\t\t<color red="0.25" green="0.93" blue="0.54" />
\t\t</disk>
\t\t<disk state="1">
\t\t\t<bounds x="0.46" y="0.22" width="0.09" height="0.56" />
\t\t\t<color red="1.00" green="0.62" blue="0.18" />
\t\t</disk>
\t\t<text state="0" string="{normal}" align="1">
\t\t\t<bounds x="0.61" y="0.12" width="0.36" height="0.76" />
\t\t\t<color red="0.63" green="0.94" blue="0.74" />
\t\t</text>
\t\t<text state="1" string="{active}" align="1">
\t\t\t<bounds x="0.61" y="0.12" width="0.36" height="0.76" />
\t\t\t<color red="1.00" green="0.78" blue="0.46" />
\t\t</text>
\t</element>'''
        for name, label, normal, active, _ in indicators
    )
    indicator_rows = "\n".join(
        f'''\t\t<element ref="indicator_{name}" name="panel_{name}_open" inputtag="SECMASK" inputmask="0x{mask:02x}">
\t\t\t{to_bounds((173, 1243 + row * 37, 431, 1280 + row * 37))}
\t\t</element>'''
        for row, (name, _, _, _, mask) in enumerate(indicators)
    )

    return f"""<?xml version="1.0"?>
<!--
license:BSD-3-Clause
copyright-holders:CoinLine contributors
-->
<mamelayout version="2">
\t<element name="front" defstate="0">
\t\t<image file="millennium-terminal-front.png" />
\t</element>
\t<element name="hit_num" defstate="0">
\t\t<rect state="0">
\t\t\t<color red="0.00" green="0.64" blue="0.91" alpha="0.30" />
\t\t</rect>
\t</element>
\t<element name="hit_down" defstate="0">
\t\t<rect state="0">
\t\t\t<color red="1.00" green="0.50" blue="0.16" alpha="0.32" />
\t\t</rect>
\t</element>
\t<element name="hit_up" defstate="0">
\t\t<rect state="0">
\t\t\t<color red="1.00" green="0.95" blue="0.00" alpha="0.32" />
\t\t</rect>
\t</element>
\t<element name="hit_lang" defstate="0">
\t\t<rect state="0">
\t\t\t<color red="0.93" green="0.11" blue="0.14" alpha="0.32" />
\t\t</rect>
\t</element>
\t<element name="hit_newcall" defstate="0">
\t\t<rect state="0">
\t\t\t<color red="0.53" green="0.00" blue="0.08" alpha="0.32" />
\t\t</rect>
\t</element>
\t<element name="hit_quick" defstate="0">
\t\t<rect state="0">
\t\t\t<color red="0.78" green="0.75" blue="0.91" alpha="0.30" />
\t\t</rect>
\t</element>
\t<element name="hit_coin" defstate="0">
\t\t<rect state="0">
\t\t\t<color red="0.85" green="0.79" blue="0.95" alpha="0.30" />
\t\t</rect>
\t</element>
\t<element name="hit_misc" defstate="0">
\t\t<rect state="0">
\t\t\t<color red="0.95" green="0.2" blue="0.95" alpha="0.00" />
\t\t</rect>
\t</element>
\t<element name="hit_vfd" defstate="0">
\t\t<rect state="0">
\t\t\t<color red="0.42" green="0.84" blue="0.97" alpha="0.00" />
\t\t</rect>
\t</element>
\t<element name="handset_button" defstate="0">
\t\t<rect>
\t\t\t<color red="0.30" green="0.36" blue="0.41" />
\t\t</rect>
\t\t<rect>
\t\t\t<bounds x="0.009" y="0.043" width="0.982" height="0.914" />
\t\t\t<color red="0.08" green="0.11" blue="0.14" />
\t\t</rect>
\t\t<disk state="0">
\t\t\t<bounds x="0.06" y="0.37" width="0.054" height="0.26" />
\t\t\t<color red="0.36" green="0.86" blue="0.61" />
\t\t</disk>
\t\t<disk state="1">
\t\t\t<bounds x="0.06" y="0.37" width="0.054" height="0.26" />
\t\t\t<color red="1.00" green="0.68" blue="0.33" />
\t\t</disk>
\t\t<text state="0" string="Lift handset" align="1">
\t\t\t<bounds x="0.16" y="0.20" width="0.61" height="0.60" />
\t\t\t<color red="0.91" green="0.94" blue="0.97" />
\t\t</text>
\t\t<text state="1" string="Hang up" align="1">
\t\t\t<bounds x="0.16" y="0.20" width="0.61" height="0.60" />
\t\t\t<color red="1.00" green="0.84" blue="0.66" />
\t\t</text>
\t\t<rect>
\t\t\t<bounds x="0.82" y="0.24" width="0.13" height="0.52" />
\t\t\t<color red="0.19" green="0.24" blue="0.29" />
\t\t</rect>
\t\t<text string="H">
\t\t\t<bounds x="0.82" y="0.29" width="0.13" height="0.42" />
\t\t\t<color red="0.81" green="0.86" blue="0.91" />
\t\t</text>
\t</element>
\t<element name="indicator_panel">
\t\t<rect>
\t\t\t<color red="0.24" green="0.29" blue="0.33" />
\t\t</rect>
\t\t<rect>
\t\t\t<bounds x="0.015" y="0.035" width="0.97" height="0.93" />
\t\t\t<color red="0.04" green="0.06" blue="0.08" />
\t\t</rect>
\t</element>
{indicator_elements}
\t<view name="Standard">
\t\t<element ref="front">
\t\t\t<bounds left="0" top="0" right="{image_w}" bottom="{image_h}" />
\t\t</element>
\t\t<screen index="0">
\t\t\t{to_bounds(vfd_box)}
\t\t</screen>
\t\t<element ref="hit_vfd">
\t\t\t{to_bounds(vfd_box)}
\t\t</element>
\t\t<!-- Numeric keypad -->
{number_elements}

\t\t<!-- Top row: down/up/language/new-call -->
{el("hit_down", "KEYMATRIX", "0x00020000", down_box)}
{el("hit_up", "KEYMATRIX", "0x00010000", up_box)}
{el("hit_lang", "KEYMATRIX", "0x00040000", lang_box)}
{el("hit_newcall", "KEYMATRIX", "0x00001000", newcall_box)}

\t\t<!-- Quick access row -->
{quick_elements}

\t\t<!-- The receiver photo and the handset button operate the same switch. -->
{el("hit_misc", "KEYMATRIX", "0x00080000", handset_box)}
{el("handset_button", "KEYMATRIX", "0x00080000", handset_button_box)}

\t\t<!-- Service toggles -->
{el("hit_misc", "SECMASK", "0x01", service_lock_box)}

\t\t<!-- Card reader: insert, read, and remove the magstripe test card -->
{el("hit_misc", "CARDUI", "0x01", card_reader_box)}

\t\t<!-- Coin slot -->
{el("hit_coin", "COINUI", "0x01", coin_slot_box)}

\t\t<element ref="indicator_panel">
\t\t\t{to_bounds((162, 1232, 442, 1366))}
\t\t</element>
{indicator_rows}
\t</view>
</mamelayout>
"""


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate millennium layout bounds from color overlay image.")
    parser.add_argument(
        "--reference",
        default="artwork/millennium-terminal-front-button-areas.png",
        help="Path to color-reference image",
    )
    parser.add_argument(
        "--output",
        nargs="*",
        default=["src/mame/layout/millennium.lay", "artwork/millennium.lay"],
        help="One or more output .lay files",
    )
    args = parser.parse_args()

    ref_path = Path(args.reference)
    img = Image.open(ref_path).convert("RGB")
    w, h = img.size
    px = img.load()
    rows: list[list[RGB]] = [[px[x, y] for x in range(w)] for y in range(h)]

    colors: dict[str, RGB] = {
        "vfd_border": (153, 217, 234),
        "num_blue": (0, 162, 232),
        "quick_lav": (200, 191, 231),
        "lang_red": (237, 28, 36),
        "newcall_darkred": (136, 0, 21),
        "down_orange": (255, 127, 39),
        "up_yellow": (255, 242, 0),
    }

    vfd_box = pop_single(find_components(rows, colors["vfd_border"], min_pixels=500), "vfd_border")
    num_boxes = normalize_numeric_keypad_order(find_components(rows, colors["num_blue"], min_pixels=500))
    quick_boxes = find_components(rows, colors["quick_lav"], min_pixels=300)
    lang_box = pop_single(find_components(rows, colors["lang_red"], min_pixels=100), "lang_red")
    newcall_box = pop_single(find_components(rows, colors["newcall_darkred"], min_pixels=100), "newcall_darkred")
    down_box = pop_single(find_components(rows, colors["down_orange"], min_pixels=100), "down_orange")
    up_candidates = find_components(rows, colors["up_yellow"], min_pixels=100)

    if len(num_boxes) != 12:
        raise RuntimeError(f"Expected 12 number-key boxes, found {len(num_boxes)}")
    if len(quick_boxes) != 5:
        raise RuntimeError(f"Expected 5 quick-access buttons, found {len(quick_boxes)}")
    if not up_candidates:
        raise RuntimeError("Expected at least one up-button yellow component")
    up_box = max(up_candidates, key=lambda b: b[0])  # right-most yellow box is UP

    xml = render_layout(
        w,
        h,
        {
            "vfd_box": vfd_box,
            "num_boxes": num_boxes,
            "quick_boxes": quick_boxes,
            "down_box": down_box,
            "up_box": up_box,
            "lang_box": lang_box,
            "newcall_box": newcall_box,
        },
    )

    for out in args.output:
        out_path = Path(out)
        out_path.parent.mkdir(parents=True, exist_ok=True)
        out_path.write_text(xml, encoding="utf-8", newline="\n")
        print(f"Wrote {out_path}")

    print("Extracted:")
    print(f"  vfd={vfd_box}")
    print(f"  down={down_box} up={up_box} lang={lang_box} newcall={newcall_box}")
    print(f"  numbers={len(num_boxes)} quick={len(quick_boxes)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
