"""Schema validation for fixtures/board/interrupt-map.json."""

from __future__ import annotations

import json
from pathlib import Path

def test_interrupt_map_schema(board_fixtures: Path) -> None:
    data = json.loads((board_fixtures / "interrupt-map.json").read_text(encoding="utf-8"))
    assert data["mode"] == "im2"
    assert data["vector_base_register"] == "I"
    assert data["il_base_register"] == "IL"
    for s in data["sources"]:
        assert "source" in s and "owning_device" in s and "evidence" in s
        if "external_line" in s and s["external_line"] is not None:
            assert isinstance(s["external_line"], str)
