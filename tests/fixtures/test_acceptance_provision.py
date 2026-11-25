"""Check generated synthetic provisioning data without a live CoinLine host."""

import importlib

import pytest


def test_craft_access_field_is_seven_digits_plus_terminator(coinline_emu_root, monkeypatch):
    monkeypatch.syspath_prepend(str(coinline_emu_root / "tools" / "acceptance"))
    provision = importlib.import_module("provision")
    assert provision.encode_craft_access("1234567") == bytes.fromhex("1234567e")
    for invalid in ("12345", "12345678", "12345a7", "123456e", None):
        with pytest.raises(ValueError):
            provision.encode_craft_access(invalid)

    monkeypatch.setattr(provision, "read_archive", lambda repo, commit, number: bytes(provision.ARCHIVE_LENGTHS[number]))
    for recipe in ("legacy", "universal", "expanded_spare", "expanded_v11"):
        first = provision.payloads(None, "test", recipe, "2025550119", "1234567", "0000000000")
        second = provision.payloads(None, "test", recipe, "2025550119", "9876543", "0000000000")
        assert len(first[31]) == len(second[31]) == 37
        assert first[31][1:5] == bytes.fromhex("1234567e")
        assert second[31][1:5] == bytes.fromhex("9876543e")
        assert first[31][:1] + first[31][5:] == second[31][:1] + second[31][5:]
        assert all(first[key] == second[key] for key in first if key != 31)
