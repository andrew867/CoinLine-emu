"""fixtures/firmware/firmware-hashes.json — hashes only, no bytes."""

from __future__ import annotations

import json
import re
from pathlib import Path

SHA256 = re.compile(r"^[0-9a-f]{64}$")

def test_firmware_hashes_shape(firmware_fixtures: Path) -> None:
    data = json.loads((firmware_fixtures / "firmware-hashes.json").read_text(encoding="utf-8"))
    entries = data["entries"]
    assert isinstance(entries, list) and len(entries) >= 1
    for e in entries:
        assert e.get("version_id")
        assert e.get("friendly_name")
        h = e["sha256_hex"].lower()
        assert SHA256.match(h), h
        assert isinstance(e["size_bytes"], int) and e["size_bytes"] > 0
        blob = json.dumps(e).lower()
        assert "beginbinary" not in blob  # sanity — no embedded binary markers
