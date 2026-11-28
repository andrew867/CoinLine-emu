# SPDX-License-Identifier: GPL-2.0-or-later
import configparser
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


def test_ordered_virtual_card_catalogue(tmp_path, monkeypatch):
    monkeypatch.setattr(launcher, "ROOT", tmp_path)
    names = ("fixtures/cards/magcard-valid.json", launcher.MAG_NEGATIVE_FIXTURE,
             launcher.SYNC_CONTACT_FIXTURE, launcher.ASYNC_CONTACT_FIXTURE, "third.json",
             launcher.SYNC_TRANSPORT_FIXTURE)
    for name in names:
        path = tmp_path / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("{}", encoding="utf-8")
    mag, smart = tmp_path / names[0], tmp_path / names[2]
    defaults = launcher.extra_card_environment({}, mag, smart)
    assert defaults == {"COINLINE_MAG_CARD_2": str(tmp_path / names[1]),
                        "COINLINE_SMART_CARD_2": str(tmp_path / names[3]),
                        "COINLINE_SMART_CARD_3": str(tmp_path / launcher.SYNC_TRANSPORT_FIXTURE)}
    alternate = launcher.extra_card_environment({}, mag, tmp_path / names[3])
    assert alternate["COINLINE_SMART_CARD_2"] == str(tmp_path / launcher.SYNC_TRANSPORT_FIXTURE)
    assert "COINLINE_SMART_CARD_3" not in alternate
    custom = launcher.extra_card_environment({"mag_fixture_3": "third.json", "smart_fixture_2": ""}, mag, smart)
    assert custom["COINLINE_MAG_CARD_3"] == str(tmp_path / "third.json")
    assert "COINLINE_SMART_CARD_2" not in custom
    for settings, error in (({"mag_fixture_2": "", "mag_fixture_3": "third.json"}, "contiguous"),
                            ({"mag_fixture_2": "absent.json"}, "missing"),
                            ({"mag_fixture_2": names[0]}, "duplicates")):
        with pytest.raises(ValueError, match=error):
            launcher.extra_card_environment(settings, mag, smart)


def test_portable_staging_includes_wallet_defaults(tmp_path, monkeypatch):
    path = PORTABLE.parents[1] / "mingw64/build-portable.py"
    pack_spec = importlib.util.spec_from_file_location("coinline_portable_packer", path)
    packer = importlib.util.module_from_spec(pack_spec)
    pack_spec.loader.exec_module(packer)
    staged = set()

    def check_staged_file(source, destination):
        name = destination.relative_to(tmp_path).as_posix()
        if name.startswith("fixtures/"):
            assert source.is_file(), name
        staged.add(name)

    monkeypatch.setattr(packer, "copy_file", check_staged_file)
    packer.stage_files(tmp_path, tmp_path, None, None, True)
    assert {"fixtures/cards/magcard-valid.json", launcher.MAG_NEGATIVE_FIXTURE,
            launcher.SYNC_CONTACT_FIXTURE, launcher.SYNC_TRANSPORT_FIXTURE,
            launcher.ASYNC_CONTACT_FIXTURE,
            "fixtures/cards/smartcard-sync-address-test.json", "fixtures/cards/smartcard-sync-gpm103-contact.json",
            "fixtures/cards/smartcard-sync-13byte-bench.json", "fixtures/cards/smartcard-sync-gpm103-lab.json",
            "fixtures/cards/smartcard-sync-gpm103-lab-readback.json",
            *launcher.LAB_PURSE_CONTACT_FIXTURES} <= staged
    assert {f"fixtures/cards/smartcard-async-lab-purse-{name}.json" for name in ("50", "500", "empty")} <= staged
    assert {"app/shadybucks.py", "fixtures/cards/shadybucks-lab-product.json"} <= staged
    assert {f"fixtures/tables/gpm103-lab/{name}" for name in (
        "README.md", "manifest.json", "maximizer-mt93-mt134-gpm103-seed.json",
        "mt022.bin", "mt023.bin", "mt093.bin", "mt134.bin", "mt135.bin",
    )} <= staged


def test_bundled_table_file_integrity():
    root = PORTABLE.parents[2] / "fixtures/tables/gpm103-lab"
    manifest = json.loads((root / "manifest.json").read_text(encoding="utf-8"))
    assert {entry["file"] for entry in manifest["entries"]} == {
        "mt022.bin", "mt023.bin", "mt093.bin", "mt134.bin", "mt135.bin",
    }
    for entry in manifest["entries"]:
        payload = (root / entry["file"]).read_bytes()
        assert len(payload) == entry["record_length"]
        assert hashlib.sha256(payload).hexdigest() == entry["sha256"]


def test_13_byte_contact_fixture_geometry():
    path = PORTABLE.parents[2] / "fixtures/cards/smartcard-sync-13byte-bench.json"
    fixture = json.loads(path.read_text(encoding="utf-8"))
    memory = bytes.fromhex(fixture["memory_hex"])
    assert fixture["protocol"] == "sync"
    assert fixture["fixture_kind"] == "non-payment-13-byte-contact-bench"
    assert memory == bytes.fromhex("96 3C A7 00 42 45 4E 43 48 AA 55 81 F0")
    assert len(memory) == 13
    assert fixture["sync_program_enabled"] is True
    assert fixture["sync_program_first_bit"] == 96
    assert fixture["sync_program_end_bit"] == 98
    assert fixture["sync_program_us"] == 10000


def test_gpm103_lab_default_fixture():
    path = PORTABLE.parents[2] / "fixtures/cards/smartcard-sync-gpm103-lab.json"
    fixture = json.loads(path.read_text(encoding="utf-8"))
    assert fixture["type"] == "smartcard"
    assert fixture["protocol"] == "sync"
    assert fixture["fixture_kind"] == "gpm103-lab-personalized"
    assert fixture["atr_hex"] == "00"
    assert fixture["memory_hex"] == "00286F00000001B978FFFFFFFF"
    assert len(bytes.fromhex(fixture["memory_hex"])) == 13
    assert fixture["sync_program_enabled"] is True
    assert fixture["sync_program_first_bit"] == 64
    assert fixture["sync_program_end_bit"] == 104
    assert fixture["sync_program_us"] == 10000


def test_gpm103_lab_readback_fixture():
    path = PORTABLE.parents[2] / "fixtures/cards/smartcard-sync-gpm103-lab-readback.json"
    fixture = json.loads(path.read_text(encoding="utf-8"))
    assert fixture["type"] == "smartcard"
    assert fixture["protocol"] == "sync"
    assert fixture["atr_hex"] == "00"
    assert fixture["memory_hex"] == "00286F00000001B978FFFFFFFF"
    assert len(bytes.fromhex(fixture["memory_hex"])) == 13
    assert not fixture.get("sync_program_enabled", False)


def test_gpm103_lab_probe_preflight_selects_read_only_fixture(tmp_path, monkeypatch):
    root = PORTABLE.parents[2]
    spec = importlib.util.spec_from_file_location(
        "coinline_smartcard_readback_probe", root / "tools/acceptance/smartcard_memory_order_probe.py")
    probe = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(probe)
    cards = tmp_path / "fixtures/cards"
    cards.mkdir(parents=True)
    for name in ("smartcard-sync-gpm103-lab.json", "smartcard-sync-gpm103-lab-readback.json"):
        (cards / name).write_bytes((root / "fixtures/cards" / name).read_bytes())
    monkeypatch.setattr(probe, "ROOT", tmp_path)
    monkeypatch.setattr(sys, "argv", ["probe", "--gpm103-lab", "--mame", str(tmp_path / "unused.exe"),
                                    "--output", str(tmp_path / "output")])
    read_paths = []
    read_text = Path.read_text

    def track_fixture_read(path, *args, **kwargs):
        if path.parent == cards:
            read_paths.append(path)
        return read_text(path, *args, **kwargs)

    monkeypatch.setattr(Path, "read_text", track_fixture_read)

    class PreflightPassed(Exception):
        pass

    def stop_before_native_run(*args, **kwargs):
        raise PreflightPassed

    monkeypatch.setattr(probe.shutil, "copytree", stop_before_native_run)
    # The writable default would fail the read-only guard. Reaching staging
    # proves that the CLI selected and validated the independent fixture.
    with pytest.raises(PreflightPassed):
        probe.main()
    assert read_paths == [cards / "smartcard-sync-gpm103-lab-readback.json"]


def test_bundled_lab_snapshots_in_untouched_card_wallet(tmp_path, monkeypatch):
    monkeypatch.setattr(launcher, "ROOT", tmp_path)
    names = ("fixtures/cards/magcard-valid.json", launcher.SYNC_CONTACT_FIXTURE,
             launcher.ASYNC_CONTACT_FIXTURE, launcher.SYNC_TRANSPORT_FIXTURE,
             *launcher.LAB_PURSE_CONTACT_FIXTURES)
    for name in names:
        path = tmp_path / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("{}", encoding="utf-8")
    mag, smart = tmp_path / names[0], tmp_path / names[1]
    defaults = launcher.extra_card_environment({}, mag, smart)
    assert [defaults[f"COINLINE_SMART_CARD_{i}"] for i in range(2, 7)] == [str(tmp_path / x) for x in names[2:]]
    alternate = launcher.extra_card_environment({}, mag, tmp_path / launcher.ASYNC_CONTACT_FIXTURE)
    assert [alternate[f"COINLINE_SMART_CARD_{i}"] for i in range(2, 6)] == [
        str(tmp_path / x) for x in (launcher.SYNC_TRANSPORT_FIXTURE, *launcher.LAB_PURSE_CONTACT_FIXTURES)]
    assert "COINLINE_SMART_CARD_6" not in alternate
    explicit = launcher.extra_card_environment({"smart_fixture_2": launcher.ASYNC_CONTACT_FIXTURE}, mag, smart)
    assert set(explicit) == {"COINLINE_SMART_CARD_2"}
    assert launcher.extra_card_environment({"smart_fixture_2": ""}, mag, smart) == {}


@pytest.mark.parametrize("primary", [launcher.SYNC_CONTACT_FIXTURE, launcher.ASYNC_CONTACT_FIXTURE])
def test_bundled_secondary_contact_slots_are_read_only(monkeypatch, primary):
    root = PORTABLE.parents[2]
    monkeypatch.setattr(launcher, "ROOT", root)
    cards = launcher.extra_card_environment({}, root / "fixtures/cards/magcard-valid.json", root / primary)
    paths = [Path(value) for key, value in cards.items() if key.startswith("COINLINE_SMART_CARD_")]
    assert paths
    assert len(paths) == len(set(paths))
    for path in paths:
        fixture = json.loads(path.read_text(encoding="utf-8"))
        assert not fixture.get("sync_program_enabled", False), path.name


@pytest.mark.parametrize("example", ["coinline.example.ini", "coinline-graphical.example.ini"])
def test_example_config_preserves_firmware_contact_defaults(tmp_path, monkeypatch, example):
    monkeypatch.setattr(launcher, "ROOT", tmp_path)
    config = configparser.ConfigParser(interpolation=None)
    config.read(PORTABLE / example, encoding="utf-8")
    args = SimpleNamespace(smart_card=None)
    for entry, expected in (({}, launcher.SYNC_CONTACT_FIXTURE),
                            ({"contact_test": "generic-async-t0"}, launcher.ASYNC_CONTACT_FIXTURE)):
        env = {"COINLINE_SMART_CARD": str(tmp_path / launcher.contact_fixture_default(entry))}
        assert launcher.customer_fixture_path(args, config["cards"], env) == tmp_path / expected


def test_coin_validator_profile_preserves_independent_board_fields(tmp_path):
    source = tmp_path / "board.json"
    original = {"coin": {"denominations": [5, 10, 25], "pulse_width_us": 350},
                "display": {"type": "graphic"}}
    source.write_text(json.dumps(original), encoding="utf-8")
    target = tmp_path / "calibrated.json"
    assert launcher.coin_profile_board(source, "board-default", target) == source
    assert not target.exists()
    assert launcher.coin_profile_board(source, "regional-nine", target) == target
    configured = json.loads(target.read_text(encoding="utf-8"))
    assert configured["coin"]["denominations"] == [5, 5, 10, 25, 100, 5, 10, 25, 100]
    assert configured["coin"]["pulse_width_us"] == 350 and configured["display"] == original["display"]
    assert json.loads(source.read_text(encoding="utf-8")) == original
    with pytest.raises(ValueError, match="Unknown coin validator profile"):
        launcher.coin_profile_board(source, "unsupported", target)


def test_posix_sip_and_direct_adsi_preflight(tmp_path, monkeypatch):
    monkeypatch.setattr(launcher, "ROOT", tmp_path)
    monkeypatch.setattr(launcher, "WINDOWS", False)
    for name in ("fixtures/cards/magcard-valid.json", launcher.SYNC_CONTACT_FIXTURE):
        path = tmp_path / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("{}", encoding="utf-8")
    config = launcher.config_for_run("oem")
    hardware = (tmp_path / "data", tmp_path / "roms", {
        "COINLINE_BOARD": "board-profile-2line-vfd.json",
        "COINLINE_MOTHERBOARD": "earlier",
        "COINLINE_SMART_CARD": str(tmp_path / launcher.SYNC_CONTACT_FIXTURE),
    }, "fixture")
    args = SimpleNamespace(profile="oem", scale=None, maximize=None, sip=True, check=True,
                           mag_card=None, smart_card=None, adsi=False)
    imported = []
    monkeypatch.setattr(launcher.importlib, "import_module", lambda name: imported.append(name))
    assert launcher.run(args, config, hardware) == 0
    assert imported == ["pyVoIP.RTP", "pyVoIP.VoIP", "pylibsrtp"]
    imported.clear()
    args.sip, args.adsi = False, True
    assert launcher.run(args, config, hardware) == 0
    assert imported == ["pyVoIP.RTP", "pyVoIP.VoIP"]
    imported.clear()
    args.adsi = False
    assert launcher.run(args, config, hardware) == 0
    assert imported == []

    def missing(name):
        raise ImportError(name)

    monkeypatch.setattr(launcher.importlib, "import_module", missing)
    args.sip = True
    with pytest.raises(ValueError, match=r"setup-sip\.sh"):
        launcher.run(args, config, hardware)
    monkeypatch.setattr(launcher, "WINDOWS", True)
    with pytest.raises(ValueError, match="bundled runtime"):
        launcher.require_sip_runtime(True)


def test_microphone_selection_and_opt_out(tmp_path, monkeypatch):
    monkeypatch.setattr(launcher, "ROOT", tmp_path)
    audio = launcher.config_for_run("oem")["audio"]
    assert launcher.microphone_environment(audio) == {}
    audio["input_device"] = "Fixture input"
    assert launcher.microphone_environment(audio) == {"COINLINE_MIC_INPUT": "Fixture input"}
    audio["capture_enabled"] = "false"
    assert launcher.microphone_environment(audio) == {"COINLINE_MIC_DISABLED": "1"}


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
