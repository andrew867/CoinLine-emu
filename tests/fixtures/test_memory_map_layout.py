"""Overlap, alignment, and required-region layout for memory-map.json."""

from __future__ import annotations

import json
from pathlib import Path

def _bases(board_fixtures: Path) -> list[tuple[str, int, int]]:
    data = json.loads((board_fixtures / "memory-map.json").read_text(encoding="utf-8"))
    out = []
    for r in data["regions"]:
        base = int(r["base"], 16)
        out.append((r["name"], base, r["size"]))
    return out

def test_no_overlap(board_fixtures: Path) -> None:
    rows = _bases(board_fixtures)
    spans = [(n, b, b + s) for n, b, s in rows if s > 0]
    for i, (n1, a1, e1) in enumerate(spans):
        for n2, a2, e2 in spans[i + 1 :]:
            if not (e1 <= a2 or e2 <= a1):
                raise AssertionError(f"overlap {n1}[{a1:#x},{e1:#x}) vs {n2}[{a2:#x},{e2:#x})")

def test_page_alignment(board_fixtures: Path) -> None:
    """Regions aligned to at least 0x1000 per memory-map.spec / MMU page."""
    for name, base, size in _bases(board_fixtures):
        if size == 0:
            continue
        assert base % 0x1000 == 0, f"{name} base {base:#x} not 4KiB-aligned"

def test_rom_at_zero(board_fixtures: Path) -> None:
    rom = next(r for r in json.loads((board_fixtures / "memory-map.json").read_text(encoding="utf-8"))["regions"] if r["name"] == "rom")
    assert rom["base"].lower() == "0x00000"
    assert rom["access"] == "r"
