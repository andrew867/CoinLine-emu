# SPDX-License-Identifier: GPL-2.0-or-later
from pathlib import Path
import sys

import pytest

ACCEPTANCE = Path(__file__).resolve().parents[2] / "tools/acceptance"
sys.path.insert(0, str(ACCEPTANCE))
from firmware_upgrade_evidence import (BLOCK_BYTES, IMAGE_BYTES, boot_qualification, digest,
                                       flash_trace_qualification, persisted,
                                       receiver_qualification,
                                       reported_version_qualification, summarize)
from run import normalize_manager_settings, validate_data_routes


def test_manager_export_settings_aliases_and_explicit_precedence():
    exported = {"wireId": "2025550100", "nccPort": 44100, "craftPin": "00000"}
    normalized = normalize_manager_settings(exported)
    assert (normalized["line"], normalized["ncc_port"], normalized["pin"]) == ("2025550100", 44100, "00000")
    assert "pin" not in exported
    explicit = normalize_manager_settings({**exported, "pin": "11111", "ncc_port": 4000})
    assert explicit["pin"] == "11111"
    assert explicit["ncc_port"] == 4000


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
    assert not report["qualified"]
    assert all(chip["before_sha256"] != chip["after_sha256"] for chip in report["chips"])
    mismatch = summarize(seed, before, after, seed, [1, 128])
    assert mismatch["expected"]["mismatched_blocks"] == [1, 128]
    with pytest.raises(ValueError, match="target image"):
        summarize(seed, before, after, expected_blocks=[1])
    with pytest.raises(ValueError, match="Block indices"):
        summarize(seed, before, after, seed, [256])


def test_complete_qualification_requires_raw_flash_restart_service_and_version(tmp_path):
    seed = bytes(IMAGE_BYTES)
    target = bytearray(seed)
    target[BLOCK_BYTES + 1] = 3
    target = bytes(target)
    trace = tmp_path / "flash.jsonl"
    trace.write_text("\n".join((
        '{"event":"erase_begin","block_4k":1}',
        '{"event":"erase_complete","block_4k":1}',
        '{"event":"program_begin","block_4k":1,"program_error":false}',
        '{"event":"program_complete","block_4k":1}',
        '{"event":"summary","program_begin":1,"program_complete":1,"erase_begin":1,"erase_complete":1}',
    )) + "\n", encoding="utf-8")
    flash = flash_trace_qualification(trace, [1])
    assert flash["qualified"]
    reported = {"source": "coinline-production", "firmware_id": "target", "version": "2.0",
                "qualified": True}
    report = summarize(seed, seed, target, target, flash_trace=flash,
                       post_restart=target, boot_ready=True, receiver_ready=True,
                       reported_version=reported)
    assert report["service_and_restart_qualified"]
    assert report["qualified"]
    missing_version = summarize(seed, seed, target, target, flash_trace=flash,
                                post_restart=target, boot_ready=True, receiver_ready=True)
    assert not missing_version["qualified"]


def test_raw_post_restart_observation_parsers(tmp_path):
    boot = tmp_path / "boot.jsonl"
    boot.write_text('{"milestone":"protocol_ready","telephony_ready":true,'
                    '"ready_sequence_completed":true}\n', encoding="utf-8")
    events = tmp_path / "events.tsv"
    events.write_text("1\tinput\thook off\n2\tinput\thook on\n", encoding="utf-8")
    keypad = tmp_path / "keypad.jsonl"
    keypad.write_text('{"event":"tp_key_event_reported_to_cp","reason":"line_connection"}\n',
                      encoding="utf-8")
    version = tmp_path / "version.json"
    version.write_text('{"source":"coinline-production","firmware_id":"target","version":"2.0"}\n',
                       encoding="utf-8")
    assert boot_qualification(boot)
    assert receiver_qualification(events, keypad)
    assert reported_version_qualification(version, "target", "2.0")["qualified"]
    assert not reported_version_qualification(version, "target", "2.1")["qualified"]


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
