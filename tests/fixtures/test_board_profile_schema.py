"""Schema validation for fixtures/board/board-profile-*.json."""

from __future__ import annotations

import json
import re
from pathlib import Path

PROFILE_ID = re.compile(r"^[a-z0-9-]+$")
HEX = re.compile(r"^0x[0-9A-Fa-f]+$")
HEX2 = re.compile(r"^0x[0-9A-Fa-f]{2}$")

def _profiles(board_fixtures: Path) -> list[Path]:
    return sorted(board_fixtures.glob("board-profile-*.json"))

def test_board_profile_files_exist(board_fixtures: Path) -> None:
    paths = _profiles(board_fixtures)
    assert len(paths) >= 2

def test_each_board_profile_schema(board_fixtures: Path) -> None:
    for path in _profiles(board_fixtures):
        data = json.loads(path.read_text(encoding="utf-8"))
        assert PROFILE_ID.match(data["profile_id"])
        d = data["display"]
        assert d["type"] == "vfd"
        assert d["variant"] in ("2line", "11line")
        assert d["columns"] >= 1 and d["rows"] >= 1
        assert data["keypad"]["layout"] in ("3x4", "4x4")
        m = data["memory"]
        assert m["rom_size"] >= 1024 and m["ram_size"] >= 1024
        assert HEX.match(m["nvram_base"])
        assert m["nvram_size"] >= 64
        assert HEX.match(m["table_storage_base"])
        assert isinstance(m["table_storage_size"], int) and m["table_storage_size"] >= 0
        assert HEX.match(m["dla_stage_base"])
        assert isinstance(m["dla_stage_size"], int) and m["dla_stage_size"] >= 0
        assert data["z180"]["clock_hz"] >= 1_000_000
        assert data["modem"]["asci_channel"] in (0, 1)
        assert data["modem"]["default_baud"] >= 300
        assert HEX2.match(data["io_ports"]["unknown_default"])
        assert data["io_ports"]["on_unknown"] in ("log_and_continue", "log_and_halt")
