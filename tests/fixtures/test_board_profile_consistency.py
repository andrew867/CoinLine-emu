"""Cross-field consistency between board profiles and memory-map.json."""

from __future__ import annotations

import json
from pathlib import Path

def test_profiles_agree_with_memory_map_fixture(board_fixtures: Path) -> None:
    mmap = json.loads((board_fixtures / "memory-map.json").read_text(encoding="utf-8"))
    rom = next(r for r in mmap["regions"] if r["name"] == "rom")
    ram = next(r for r in mmap["regions"] if r["name"] == "ram")
    nv = next(r for r in mmap["regions"] if r["name"] == "nvram")
    ts = next(r for r in mmap["regions"] if r["name"] == "tablestore")
    dl = next(r for r in mmap["regions"] if r["name"] == "dlastage")

    for path in sorted(board_fixtures.glob("board-profile-*.json")):
        m = json.loads(path.read_text(encoding="utf-8"))["memory"]
        assert m["rom_size"] == rom["size"]
        assert m["ram_size"] == ram["size"]
        assert int(m["nvram_base"], 16) == int(nv["base"], 16)
        assert m["nvram_size"] == nv["size"]
        assert int(m["table_storage_base"], 16) == int(ts["base"], 16)
        assert m["table_storage_size"] == ts["size"]
        assert int(m["dla_stage_base"], 16) == int(dl["base"], 16)
        assert m["dla_stage_size"] == dl["size"]

def test_eleven_line_has_more_quick_keys(board_fixtures: Path) -> None:
    p2 = json.loads((board_fixtures / "board-profile-2line-vfd.json").read_text(encoding="utf-8"))
    p11 = json.loads((board_fixtures / "board-profile-11line-vfd.json").read_text(encoding="utf-8"))
    assert p11["display"]["variant"] == "11line"
    assert len(p2["keypad"]["quick_access_keys"]) == 5
    assert len(p11["keypad"]["quick_access_keys"]) == 10
    assert len(p11["keypad"]["quick_access_keys"]) > len(p2["keypad"]["quick_access_keys"])
