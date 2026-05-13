#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Emit fixtures/nvram/all-zero.nvram.json — 8192 zero bytes, sum8 checksum 0x00."""
from __future__ import annotations

import base64
import json
import sys
from pathlib import Path

def main() -> int:
    root = Path(__file__).resolve().parents[2]
    out = root / "fixtures" / "nvram" / "all-zero.nvram.json"
    size = 8192
    raw = bytes(size)
    if sum(raw) & 0xFF:
        return 2
    doc = {
        "version": "1.0",
        "size": size,
        "checksum_algorithm": "sum8",
        "checksum_value": "0x00",
        "data_b64": base64.b64encode(raw).decode("ascii"),
    }
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(doc, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {out}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
