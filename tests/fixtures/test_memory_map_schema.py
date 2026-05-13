"""Schema validation for fixtures/board/memory-map.json (Tranche E1)."""

from __future__ import annotations

import json
import re
from pathlib import Path

import pytest

HEX = re.compile(r"^0x[0-9A-Fa-f]+$")

def _load(p: Path) -> dict:
    return json.loads(p.read_text(encoding="utf-8"))

def test_memory_map_schema(board_fixtures: Path) -> None:
    data = _load(board_fixtures / "memory-map.json")
    assert isinstance(data.get("address_spaces"), list)
    assert "program" in data["address_spaces"]
    regions = data["regions"]
    assert isinstance(regions, list)
    names = {r["name"] for r in regions}
    for req in ("rom", "ram", "nvram", "tablestore", "dlastage"):
        assert req in names, f"missing required region {req}"
    for r in regions:
        assert HEX.match(r["base"]), r
        assert isinstance(r["size"], int) and r["size"] >= 0
        assert r["access"] in ("r", "w", "rw")
        assert isinstance(r["device"], str) and r["device"]
