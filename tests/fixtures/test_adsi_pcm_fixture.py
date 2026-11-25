"""Deterministic analogue acceptance stimulus geometry and integrity."""

import hashlib
import importlib
import struct


def test_single_connect_stimulus(coinline_emu_root, monkeypatch):
    monkeypatch.syspath_prepend(str(coinline_emu_root / "tools" / "acceptance"))
    fixture = importlib.import_module("adsi_pcm_fixture")
    raw, meta = fixture.stimulus()
    samples = [item[0] for item in struct.iter_unpack("<h", raw)]
    assert meta["sample_rate"] == 8000
    assert len(samples) == meta["samples"]
    assert meta["cas_end"] - meta["cas_start"] == 680
    assert meta["fsk_segment_start"] - meta["cas_end"] == 320 * 8
    assert meta["fsk_segment_end"] - meta["fsk_segment_start"] == 1814
    assert all(x == 0 for x in samples[:meta["cas_start"]])
    assert any(x != 0 for x in samples[meta["cas_start"]:meta["cas_end"]])
    assert all(x == 0 for x in samples[meta["cas_end"]:meta["fsk_segment_start"]])
    assert len(samples[meta["fsk_segment_end"]:]) == 16000
    assert all(x == 0 for x in samples[meta["fsk_segment_end"]:])
    assert sum(fixture.CONNECT) % 256 == 0
    assert hashlib.sha256(raw).hexdigest() == meta["sha256"]
    assert fixture.stimulus()[0] == raw


def test_captured_connect_is_unmodified(coinline_emu_root, monkeypatch):
    monkeypatch.syspath_prepend(str(coinline_emu_root / "tools" / "acceptance"))
    fixture = importlib.import_module("adsi_pcm_fixture")
    captured = struct.pack("<hhh", -32768, 0, 32767)
    raw, meta = fixture.stimulus(100, captured)
    assert raw[2 * meta["fsk_segment_start"]:2 * meta["fsk_segment_end"]] == captured
