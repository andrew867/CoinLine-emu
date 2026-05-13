"""Non-overlap of memory regions declared in each board profile."""

from __future__ import annotations

import json
from pathlib import Path

def _profiles(board_fixtures: Path) -> list[Path]:
    return sorted(board_fixtures.glob("board-profile-*.json"))

def _spans(m: dict) -> list[tuple[str, int, int]]:
    def b(s: str) -> int:
        return int(s, 16)

    nv = b(m["nvram_base"])
    ts = b(m["table_storage_base"])
    dl = b(m["dla_stage_base"])
    return [
        ("nvram", nv, nv + m["nvram_size"]),
        ("table_storage", ts, ts + m["table_storage_size"]),
        ("dla_stage", dl, dl + m["dla_stage_size"]),
    ]

def test_board_profile_memory_nonoverlap(board_fixtures: Path) -> None:
    for path in _profiles(board_fixtures):
        m = json.loads(path.read_text(encoding="utf-8"))["memory"]
        spans = [(n, a, e) for n, a, e in _spans(m) if e > a]
        for i, (n1, a1, e1) in enumerate(spans):
            for n2, a2, e2 in spans[i + 1 :]:
                assert e1 <= a2 or e2 <= a1, f"{path.name}: overlap {n1} vs {n2}"
