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


def test_shared_adsi_routes_preserve_profile_sip_and_validate_exact_mapping(tmp_path, monkeypatch):
    monkeypatch.setattr(launcher, "ROOT", tmp_path)
    (tmp_path / "coinline.ini").write_text("[sip]\nusername=oem\npassword=fixture\n[adsi]\nenabled=false\n", encoding="utf-8")
    (tmp_path / "coinline-graphical.ini").write_text("[sip]\nusername=graphical\n[adsi]\nenabled=false\n", encoding="utf-8")
    shared = tmp_path / launcher.ADSI_SHARED_CONFIG
    shared.write_text("[adsi]\nenabled=true\nhost=10.1.2.3\nport=5070\n[adsi.routes]\n"
                      "2374=2374,7095550174\n6501=6501,17095550161\n", encoding="utf-8")
    args = SimpleNamespace(adsi=False)
    for profile in ("oem", "graphical"):
        config = launcher.config_for_run(profile)
        assert config["sip"]["username"] == profile
        assert launcher.adsi_route_settings(args, config) == [
            {"digits": ["2374", "7095550174"], "target": "2374", "host": "10.1.2.3", "port": 5070},
            {"digits": ["6501", "17095550161"], "target": "6501", "host": "10.1.2.3", "port": 5070}]
    config["adsi.routes"]["6501"] = "2374"
    with pytest.raises(ValueError, match="unique"):
        launcher.adsi_route_settings(args, config)
    config["adsi.routes"]["6501"] = "6501"
    config["adsi"]["host"] = "8.8.8.8"
    with pytest.raises(ValueError, match="private IPv4"):
        launcher.adsi_route_settings(args, config)
    shared.write_text("[adsi]\nenabled=true\n[sip]\npassword=not-a-service-setting\n", encoding="utf-8")
    with pytest.raises(ValueError, match="only"):
        launcher.config_for_run("oem")


def test_host_playback_and_scheduling_settings(tmp_path, monkeypatch):
    monkeypatch.setattr(launcher, "ROOT", tmp_path)
    config = launcher.config_for_run("graphical")
    args = SimpleNamespace(audio_buffer_ms=None, max_frameskip=None, host_priority=None)
    assert launcher.performance_settings(args, config) == (240, 10, "above-normal")
    config["display"]["max_frameskip"] = "11"
    assert launcher.performance_settings(args, config) == (240, 10, "above-normal")
    assert config["display"]["max_frameskip"] == "11"
    assert launcher.performance_settings(SimpleNamespace(max_frameskip=11), config) == (240, 10, "above-normal")
    config["audio"]["playback_buffer_ms"] = "120"
    config["display"]["max_frameskip"] = "4"
    config["performance"]["host_priority"] = "normal"
    assert launcher.performance_settings(args, config) == (120, 4, "normal")
    args.audio_buffer_ms, args.max_frameskip, args.host_priority = 300, 8, "above-normal"
    assert launcher.performance_settings(args, config) == (300, 8, "above-normal")
    with pytest.raises(ValueError, match="playback_buffer_ms"):
        launcher.performance_settings(SimpleNamespace(audio_buffer_ms=0), config)
    with pytest.raises(ValueError, match="max_frameskip"):
        launcher.performance_settings(SimpleNamespace(max_frameskip=12), config)
    with pytest.raises(ValueError, match="max_frameskip"):
        launcher.performance_settings(SimpleNamespace(max_frameskip=-1), config)
    with pytest.raises(ValueError, match="host_priority"):
        launcher.performance_settings(SimpleNamespace(host_priority="realtime"), config)


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


def test_internal_contact_module_fixture_options_are_profile_scoped(tmp_path, monkeypatch):
    monkeypatch.setattr(launcher, "ROOT", tmp_path)
    for name in ("slot0.json", "slot1.json", "override.json"):
        (tmp_path / name).write_text(json.dumps({"protocol": "async", "application": "lab-readonly", "atr_hex": "3B 00"}))
    (tmp_path / "coinline-graphical.ini").write_text("[cards]\nsam_slot0=slot0.json\nsam_slot1=slot1.json\n")
    args = SimpleNamespace(sam_slot0=None, sam_slot1=Path("override.json"), sam_slot2=None)
    config = launcher.config_for_run("graphical")
    result = launcher.module_fixture_environment(args, config["cards"], "later")
    assert result == {"COINLINE_SAM_SLOT0": str(tmp_path / "slot0.json"),
                      "COINLINE_SAM_SLOT1": str(tmp_path / "override.json")}
    assert launcher.module_fixture_environment(SimpleNamespace(), launcher.config_for_run("oem")["cards"], "earlier") == {}
    with pytest.raises(ValueError, match="later motherboard"):
        launcher.module_fixture_environment(args, config["cards"], "earlier")
    (tmp_path / "bad.json").write_text(json.dumps({"protocol": "sync"}))
    args.sam_slot1 = Path("bad.json")
    with pytest.raises(ValueError, match="read-only async"):
        launcher.module_fixture_environment(args, config["cards"], "later")
    (tmp_path / "bad.json").write_text(json.dumps({"protocol": "async", "application": "lab-readonly", "atr_hex": "XX"}))
    with pytest.raises(ValueError, match="invalid fixture parameters"):
        launcher.module_fixture_environment(args, config["cards"], "later")
    args.sam_slot1 = Path("missing.json")
    with pytest.raises(ValueError, match="missing or invalid"):
        launcher.module_fixture_environment(args, config["cards"], "later")


def test_data_jack_profile_is_explicit_and_later_board_only():
    args = SimpleNamespace(data_jack=None)
    assert launcher.data_jack_environment(args, {}, "earlier") == {}
    settings = {"data_jack_profile": "desktop", "data_jack_monitor_polarity": "low"}
    assert launcher.data_jack_environment(args, settings, "later") == {
        "COINLINE_DATA_JACK_PROFILE": "desktop", "COINLINE_DATA_JACK_MONITOR_POLARITY": "low"}
    with pytest.raises(ValueError, match="later motherboard"):
        launcher.data_jack_environment(args, settings, "earlier")
    args.data_jack = "disabled"
    assert launcher.data_jack_environment(args, settings, "earlier") == {}
    args.data_jack = "downloaded"
    with pytest.raises(ValueError, match="polarity"):
        launcher.data_jack_environment(args, {"data_jack_monitor_polarity": "invalid"}, "later")


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
    assert oem_env["COINLINE_SMART_CARD"] == str(tmp_path / launcher.SYNC_CONTACT_FIXTURE)
    assert graphic_env["COINLINE_SMART_CARD"] == str(tmp_path / launcher.SYNC_CONTACT_FIXTURE)
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
                                 ("NLA1X09", "full"), ("NLA1X09", "full-service-fix"),
                                 ("NLA1X09", "full-current"), ("NLA1X09", "full-routing")):
        image = f"<{firmware_id}V1.3>{variant}".encode().ljust(0x100000, b"\xff")
        path = tmp_path / "firmware" / f"{firmware_id}-{variant}.bin"
        path.parent.mkdir(exist_ok=True)
        path.write_bytes(image)
        entry = {"version_id": firmware_id + "-" + variant, "firmware_id": firmware_id,
                 "motherboard": "later", "display": "11line",
                 "sha256_hex": hashlib.sha256(image).hexdigest()}
        if variant in ("full-current", "full-routing"):
            entry["contact_test"] = "generic-async-t0"
        entries.append(entry)
        registry.write_text(json.dumps({"entries": entries}), encoding="utf-8")
        args = SimpleNamespace(profile="graphical", firmware=path, flash1=None,
                               rompath=None, display=None, hardware=None, portable_display=None)
        state, _, env, _ = launcher.prepare_hardware(args, launcher.config_for_run("graphical"))
        assert env["COINLINE_HARDWARE_PRESET"] == "multipay-v1.3"
        assert env["COINLINE_CARD_DMA_CHANNEL"] == "1"
        assert env["COINLINE_BOARD"].endswith("board-profile-11line-vfd.json")
        default = launcher.ASYNC_CONTACT_FIXTURE if variant in ("full-current", "full-routing") else launcher.SYNC_CONTACT_FIXTURE
        assert env["COINLINE_SMART_CARD"] == str(tmp_path / default)
        assert launcher.select_hardware("auto", image) == ("multipay-v1.3", "program tag")
        states.append(state)
    assert len(set(states)) == len(states)


def test_contact_fixture_defaults_and_explicit_overrides(tmp_path, monkeypatch):
    monkeypatch.setattr(launcher, "ROOT", tmp_path)
    hardware_env = {"COINLINE_SMART_CARD": str(tmp_path / launcher.ASYNC_CONTACT_FIXTURE)}
    args = SimpleNamespace(smart_card=None)
    assert launcher.customer_fixture_path(args, {}, hardware_env) == tmp_path / launcher.ASYNC_CONTACT_FIXTURE
    assert launcher.customer_fixture_path(args, {"smart_fixture": launcher.SYNC_CONTACT_FIXTURE}, hardware_env) == tmp_path / launcher.SYNC_CONTACT_FIXTURE
    args.smart_card = "custom.json"
    assert launcher.customer_fixture_path(args, {"smart_fixture": launcher.SYNC_CONTACT_FIXTURE}, hardware_env) == tmp_path / "custom.json"
    assert launcher.module_fixture_environment(SimpleNamespace(), {}, "later") == {}
