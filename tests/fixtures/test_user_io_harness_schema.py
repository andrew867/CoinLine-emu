"""Schema checks for user-io harness example fixtures."""

from __future__ import annotations

import json
from pathlib import Path

def test_user_io_harness_input_example_schema(coinline_emu_root: Path) -> None:
    path = coinline_emu_root / "fixtures" / "harness" / "user-io-harness-input.example.json"
    data = json.loads(path.read_text(encoding="utf-8"))
    assert isinstance(data["run_id"], str) and data["run_id"]
    profile = data["profile_traits"]
    assert profile["quick_access_key_count"] in (5, 10)
    assert isinstance(profile["has_11_line_softkeys"], bool)
    vectors = data["enabled_vectors"]
    assert isinstance(vectors, list) and vectors

def test_user_io_evidence_summary_example_schema(coinline_emu_root: Path) -> None:
    path = coinline_emu_root / "fixtures" / "harness" / "user-io-evidence-summary.example.json"
    data = json.loads(path.read_text(encoding="utf-8"))
    assert isinstance(data["run_id"], str) and data["run_id"]
    assert data["resolved_profile_id"] in ("repdial_5", "repdial_10", "vfd_11line_softkeys")
    assert isinstance(data["spec_versions"], dict) and data["spec_versions"]
    assert isinstance(data["vector_results"], list)
    assert isinstance(data["counters"], dict)
    assert isinstance(data["pass_fail_summary"], dict)
