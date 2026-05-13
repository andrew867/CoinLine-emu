"""Schema validation for fixtures/board/io-port-map.json."""

from __future__ import annotations

import json
import re
from pathlib import Path

import pytest

HEX2 = re.compile(r"^0x[0-9A-Fa-f]{2}$")

def test_io_port_map_schema(board_fixtures: Path) -> None:
    data = json.loads((board_fixtures / "io-port-map.json").read_text(encoding="utf-8"))
    assert HEX2.match(data["unknown_default"])
    assert data.get("on_unknown") in (None, "log_and_continue", "log_and_halt")
    ports = data["ports"]
    assert isinstance(ports, list) and ports
    seen: set[str] = set()
    for p in ports:
        assert HEX2.match(p["port"]), p
        assert p["port"] not in seen, f"duplicate port {p['port']}"
        seen.add(p["port"])
        assert p["direction"] in ("r", "w", "rw")
        assert p["device"]
        assert p["status"] in ("known", "suspected", "unknown")
        if "default" in p:
            assert HEX2.match(p["default"])
