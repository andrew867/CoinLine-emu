# SPDX-License-Identifier: GPL-2.0-or-later
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
from types import SimpleNamespace

import pytest

PORTABLE = Path(__file__).resolve().parents[2] / "tools/windows/portable"
sys.path.insert(0, str(PORTABLE))
spec = importlib.util.spec_from_file_location("coinline_portable_launcher", PORTABLE / "launch.py")
launcher = importlib.util.module_from_spec(spec)
spec.loader.exec_module(launcher)


def test_independent_profile_config_and_sip_ports(tmp_path, monkeypatch):
    monkeypatch.setattr(launcher, "ROOT", tmp_path)
    common = "[sip]\nregistrar=localhost\npassword=test\nlocal_address=127.0.0.1\n"
    (tmp_path / "coinline.ini").write_text(common + "username=two-line\n", encoding="utf-8")
    (tmp_path / "coinline-graphical.ini").write_text(common + "username=graphical\n", encoding="utf-8")
    oem, graphical = {}, {}
    launcher.configure_sip(oem, launcher.config_for_run("oem")["sip"], "oem")
    launcher.configure_sip(graphical, launcher.config_for_run("graphical")["sip"], "graphical")
    assert oem["COINLINE_SIP_USER"] == "two-line"
    assert graphical["COINLINE_SIP_USER"] == "graphical"
    assert (oem["COINLINE_SIP_LOCAL_PORT"], graphical["COINLINE_SIP_LOCAL_PORT"]) == ("5087", "5088")
    assert int(oem["COINLINE_SIP_RTP_PORT_HIGH"]) < int(graphical["COINLINE_SIP_RTP_PORT_LOW"])


def test_windows_scale_and_maximize_options(tmp_path, monkeypatch):
    monkeypatch.setattr(launcher, "ROOT", tmp_path)
    (tmp_path / "coinline-graphical.ini").write_text("[display]\nwindow_scale=2\nwindow_maximized=false\n", encoding="utf-8")
    args = SimpleNamespace(scale=None, maximize=None)
    assert launcher.window_settings(args, launcher.config_for_run("oem")) == ("360x850", False)
    assert launcher.window_settings(args, launcher.config_for_run("graphical")) == ("720x1700", True)
    args.scale = 1.5
    assert launcher.window_settings(args, launcher.config_for_run("graphical")) == ("540x1275", True)
    args.maximize = True
    assert launcher.window_settings(args, launcher.config_for_run("graphical")) == (None, True)
    args.scale = 0
    with pytest.raises(ValueError, match="window_scale"):
        launcher.window_settings(args, launcher.config_for_run("oem"))


def test_profile_default_programs_display_and_state(tmp_path, monkeypatch):
    monkeypatch.setattr(launcher, "ROOT", tmp_path)
    monkeypatch.setattr(launcher, "check_files", lambda *_: None)
    oem = bytes(0x40000)
    graphics = bytes([5]) * 0x100000
    for relative, image in (("firmware/oem/flash.bin", oem), ("firmware/graphical/flash.bin", graphics)):
        path = tmp_path / relative
        path.parent.mkdir(parents=True)
        path.write_bytes(image)
    registry = tmp_path / "fixtures/firmware/firmware-hashes.json"
    registry.parent.mkdir(parents=True)
    registry.write_text(json.dumps({"entries": [
        {"version_id": "mtr19-universal", "firmware_id": "NBA1F02", "motherboard": "earlier",
         "sha256_hex": hashlib.sha256(oem.ljust(0x100000, b"\xff")).hexdigest()},
        {"version_id": "graphical-test", "firmware_id": "NLA1X08", "motherboard": "later", "display": "11line",
         "sha256_hex": hashlib.sha256(graphics).hexdigest()},
    ]}), encoding="utf-8")

    def args(profile, firmware=None):
        return SimpleNamespace(profile=profile, firmware=firmware, flash1=None, rompath=None,
                               display=None, hardware=None, portable_display=None)

    oem_state, _, oem_env, _ = launcher.prepare_hardware(args("oem"), launcher.config_for_run("oem"))
    graphic_state, _, graphic_env, _ = launcher.prepare_hardware(args("graphical"), launcher.config_for_run("graphical"))
    assert oem_state == tmp_path / "data/oem"
    assert graphic_state.is_relative_to(tmp_path / "data/graphical")
    assert graphic_state.name == "11line"
    assert oem_env["COINLINE_MOTHERBOARD"] == "earlier"
    assert graphic_env["COINLINE_MOTHERBOARD"] == "later"
    assert graphic_env["COINLINE_HARDWARE_PRESET"] == "multipay-v1.3"
    assert graphic_env["COINLINE_BOARD"].endswith("board-profile-11line-vfd.json")
    with pytest.raises(ValueError, match="registered 11-line"):
        launcher.prepare_hardware(args("graphical", Path("firmware/oem/flash.bin")), launcher.config_for_run("graphical"))


def test_proton_variant_hardware_and_distinct_battery(tmp_path, monkeypatch):
    monkeypatch.setattr(launcher, "ROOT", tmp_path)
    monkeypatch.setattr(launcher, "check_files", lambda *_: None)
    registry = tmp_path / "fixtures/firmware/firmware-hashes.json"
    registry.parent.mkdir(parents=True)
    entries = []
    states = []
    for firmware_id, variant in (("NLA1X08", "original"), ("NLA1X09", "focused"),
                                 ("NLA1X09", "full"), ("NLA1X09", "full-service-fix")):
        image = f"<{firmware_id}V1.3>{variant}".encode().ljust(0x100000, b"\xff")
        path = tmp_path / "firmware" / f"{firmware_id}-{variant}.bin"
        path.parent.mkdir(exist_ok=True)
        path.write_bytes(image)
        entries.append({"version_id": firmware_id + "-" + variant, "firmware_id": firmware_id,
                        "motherboard": "later", "display": "11line",
                        "sha256_hex": hashlib.sha256(image).hexdigest()})
        registry.write_text(json.dumps({"entries": entries}), encoding="utf-8")
        args = SimpleNamespace(profile="graphical", firmware=path, flash1=None,
                               rompath=None, display=None, hardware=None, portable_display=None)
        state, _, env, _ = launcher.prepare_hardware(args, launcher.config_for_run("graphical"))
        assert env["COINLINE_HARDWARE_PRESET"] == "multipay-v1.3"
        assert env["COINLINE_CARD_DMA_CHANNEL"] == "1"
        assert env["COINLINE_BOARD"].endswith("board-profile-11line-vfd.json")
        assert launcher.select_hardware("auto", image) == ("multipay-v1.3", "program tag")
        states.append(state)
    assert len(set(states)) == len(states)
