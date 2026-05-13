"""Paths for Tranche E1 fixture schema tests."""

from pathlib import Path

import pytest

@pytest.fixture(scope="session")
def coinline_emu_root() -> Path:
    return Path(__file__).resolve().parents[2]

@pytest.fixture(scope="session")
def board_fixtures(coinline_emu_root: Path) -> Path:
    return coinline_emu_root / "fixtures" / "board"

@pytest.fixture(scope="session")
def firmware_fixtures(coinline_emu_root: Path) -> Path:
    return coinline_emu_root / "fixtures" / "firmware"
