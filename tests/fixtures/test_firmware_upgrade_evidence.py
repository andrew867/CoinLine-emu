# SPDX-License-Identifier: GPL-2.0-or-later
from pathlib import Path
import sys

import pytest

ACCEPTANCE = Path(__file__).resolve().parents[2] / "tools/acceptance"
sys.path.insert(0, str(ACCEPTANCE))
from firmware_upgrade_evidence import BLOCK_BYTES, IMAGE_BYTES, digest, persisted, summarize
from run import validate_data_routes


def test_upgrade_block_comparison_and_seed_binding(tmp_path):
    seed = bytes(IMAGE_BYTES)
    before = bytearray(seed)
    before[17] = 1
    after = bytearray(before)
    after[BLOCK_BYTES + 17] = 2
    after[128 * BLOCK_BYTES + 2] = 3
    file = tmp_path / "program_flash"
    file.write_bytes(digest(seed).encode("ascii") + after)
    assert persisted(file, digest(seed)) == after
    with pytest.raises(ValueError, match="seed binding"):
        persisted(file, digest(bytes([1]) * IMAGE_BYTES))
    report = summarize(seed, before, after, after, [1])
    assert report["changed_blocks"] == [1, 128]
    assert report["expected"]["matches_requested_blocks"]
    assert report["expected"]["changed_outside_requested_blocks"] == [128]
    assert not report["service_and_restart_qualified"]
    assert all(chip["before_sha256"] != chip["after_sha256"] for chip in report["chips"])
    mismatch = summarize(seed, before, after, seed, [1, 128])
    assert mismatch["expected"]["mismatched_blocks"] == [1, 128]
    with pytest.raises(ValueError, match="target image"):
        summarize(seed, before, after, expected_blocks=[1])
    with pytest.raises(ValueError, match="Block indices"):
        summarize(seed, before, after, seed, [256])


@pytest.mark.parametrize("settings", [
    {"ncc_port": 0}, {"code_server_number": "2130706433", "code_server_port": 4001},
    {"code_server_number": "2130706434"}, {"code_server_port": 4001},
    {"code_server_number": "2130706434", "code_server_port": True},
])
def test_invalid_code_server_routes(settings):
    with pytest.raises(ValueError):
        validate_data_routes(settings)


def test_optional_distinct_code_server_alias():
    assert validate_data_routes({}) == {"ncc_port": 4000}
    settings = {"code_server_number": "2130706434", "code_server_port": 4001}
    assert validate_data_routes(settings) == {**settings, "ncc_port": 4000}
