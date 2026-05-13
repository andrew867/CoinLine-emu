"""Schema validation for fixtures/board/device-map.json."""

from __future__ import annotations

import json
from pathlib import Path

def test_device_map_schema(board_fixtures: Path) -> None:
    data = json.loads((board_fixtures / "device-map.json").read_text(encoding="utf-8"))
    assert data.get("version") == 1
    devs = data["devices"]
    assert isinstance(devs, list) and devs
    tags: set[str] = set()
    for d in devs:
        assert d["tag"] and d["tag"] not in tags
        tags.add(d["tag"])
        assert d["device_class"]
        assert isinstance(d["owning_ports"], list)
        for op in d["owning_ports"]:
            assert isinstance(op, str) and op
