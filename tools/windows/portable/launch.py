# SPDX-License-Identifier: GPL-2.0-or-later
import argparse
import configparser
from datetime import datetime, timezone
import hashlib
import importlib
from ipaddress import IPv4Address, IPv4Network
import json
import os
from pathlib import Path
import re
import socket
import subprocess
import sys

from hardware import PRESETS, select_hardware


ROOT = Path(__file__).resolve().parent.parent
WINDOWS = os.name == "nt"
EXECUTABLE = "bin/coinline-mame.exe" if WINDOWS else "bin/coinline-mame"
PROFILES = {
    "oem": ("coinline.ini", "firmware/oem/flash.bin", "auto", 5087, 10000, 14999),
    "graphical": ("coinline-graphical.ini", "firmware/graphical/flash.bin", "11line", 5088, 15000, 19999),
}
SIP_CREDENTIAL_FIELDS = {"registrar": "HOST", "username": "USER", "password": "PASSWORD"}
SYNC_CONTACT_FIXTURE = "fixtures/cards/smartcard-sync-test.json"
ASYNC_CONTACT_FIXTURE = "fixtures/cards/smartcard-async-contact-atr-lab.json"
ADSI_SHARED_CONFIG = "adsi-routes.ini"
ADSI_CONFIG_SECTIONS = ("adsi", "adsi.routes")
ADSI_CONFIG_KEYS = {"enabled", "host", "port", "dial_digits", "target"}


def config_for_run(profile):
    config = configparser.ConfigParser(interpolation=None)
    config.read(ROOT / PROFILES[profile][0], encoding="utf-8-sig")
    shared = ROOT / ADSI_SHARED_CONFIG
    if shared.is_file():
        # This deliberately separate service-only file can be replaced without
        # overwriting either profile's own SIP credentials or terminal data.
        service = read_adsi_config(shared)
        for section in ADSI_CONFIG_SECTIONS:
            config.remove_section(section)
        config.read_dict({section: dict(service[section]) for section in service.sections()})
    for section in ("line", "sip", "hardware", "cards", "adsi", "adsi.routes", "display", "audio", "performance"):
        if section not in config:
            config.add_section(section)
    return config


def read_adsi_config(path):
    service = configparser.ConfigParser(interpolation=None)
    with Path(path).open(encoding="utf-8-sig") as source:
        service.read_file(source)
    if (service.defaults() or "adsi" not in service or
            any(section not in ADSI_CONFIG_SECTIONS for section in service.sections()) or
            set(service["adsi"]) - ADSI_CONFIG_KEYS):
        raise ValueError("adsi-routes.ini may contain only the [adsi] and [adsi.routes] service settings.")
    return service


def adsi_route_settings(args, config):
    settings = config["adsi"]
    if not (getattr(args, "adsi", False) or settings.getboolean("enabled", fallback=False)):
        return None
    host = IPv4Address(settings.get("host", "127.0.0.1").strip())
    if not (host.is_loopback or any(host in IPv4Network(network) for network in
                                    ("10.0.0.0/8", "172.16.0.0/12", "192.168.0.0/16"))):
        raise ValueError("Set a private IPv4 host under [adsi].")
    port = settings.getint("port", fallback=5069)
    if not 1 <= port <= 65535:
        raise ValueError("Set a valid UDP port under [adsi].")
    mapping = config["adsi.routes"] if "adsi.routes" in config else {}
    pairs = list(mapping.items()) if mapping else [(settings.get("target", "2374"),
                  settings.get("dial_digits", "2374,7095550174,17095550174"))]
    if not pairs or len(pairs) > 128:
        raise ValueError("Set a valid exact local ADSI route list.")
    routes, seen = [], set()
    for target, values in pairs:
        target = target.strip()
        aliases = [value.strip() for value in values.split(",")]
        if (not re.fullmatch(r"[0-9]{1,15}", target) or
                any(not re.fullmatch(r"[0-9]{1,15}", digit) or digit == "2130706433" for digit in aliases) or
                len(set(aliases)) != len(aliases) or seen.intersection(aliases)):
            raise ValueError("Set unique, exact numeric aliases and targets under [adsi.routes].")
        seen.update(aliases)
        routes.append({"digits": aliases, "target": target, "host": str(host), "port": port})
    return routes


def window_settings(args, config):
    settings = config["display"]
    scale = args.scale if args.scale is not None else settings.getfloat("window_scale", 1.0)
    if not 0.5 <= scale <= 4.0:
        raise ValueError("Set window_scale or --scale between 0.5 and 4.0.")
    maximize = args.maximize if args.maximize is not None else settings.getboolean("window_maximized", False)
    # MAME treats -resolution as the window's maximum, not its initial
    # client size. Start at that bound for an explicit scale; an unrestricted
    # maximize uses the monitor's available work area instead.
    if maximize:
        return None, True
    return f"{round(360 * scale)}x{round(850 * scale)}", scale != 1.0


def performance_settings(args, config):
    # MAME's output latency is independent of the terminal's analogue sample
    # clock, UART timing and CO/RTP playout. Automatic frame skipping sheds
    # artwork work first; no fixed frame skip or emulation speed change.
    buffer_ms = getattr(args, "audio_buffer_ms", None)
    if buffer_ms is None:
        buffer_ms = config["audio"].getint("playback_buffer_ms", 240)
    max_skip = getattr(args, "max_frameskip", None)
    if max_skip is None:
        max_skip = config["display"].getint("max_frameskip", 11)
    priority = getattr(args, "host_priority", None) or config["performance"].get("host_priority", "above-normal").strip()
    if not 40 <= buffer_ms <= 500:
        raise ValueError("Set playback_buffer_ms or --audio-buffer-ms between 40 and 500.")
    if not 0 <= max_skip <= 11:
        raise ValueError("Set max_frameskip or --max-frameskip between 0 and 11.")
    if priority not in ("normal", "above-normal"):
        raise ValueError("Set host_priority to normal or above-normal.")
    return buffer_ms, max_skip, priority


def check_files(program, rompath, display):
    files = [EXECUTABLE, "firmware/tp_pcd3349a_behavioural.rom",
             f"fixtures/board/board-profile-{display}-vfd.json", "fixtures/firmware/firmware-hashes.json",
             "fixtures/cards/magcard-valid.json", "fixtures/cards/smartcard-sync-test.json", "app/central_office.py",
             "app/sip_media.py"]
    paths = [*(ROOT / name for name in files), *program,
             *(rompath / "cl_millennium" / name for name in ("voice_u16.bin", "voice_u26.bin"))]
    missing = [str(path) for path in paths if not path.is_file()]
    if missing:
        raise ValueError("Missing package files. See Quick Start.txt:\n  " + "\n  ".join(missing))


def get_line_port():
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        return str(probe.getsockname()[1])


def contact_fixture_default(entry):
    # This is a launch-time choice of generic test contact, based on an
    # explicitly qualified image, not a native response or payment profile.
    return ASYNC_CONTACT_FIXTURE if entry.get("contact_test") == "generic-async-t0" else SYNC_CONTACT_FIXTURE


def customer_fixture_path(args, cards, hardware_env):
    return ROOT / (args.smart_card or cards.get("smart_fixture", "").strip()
                   or hardware_env.get("COINLINE_SMART_CARD", SYNC_CONTACT_FIXTURE))


def module_fixture_environment(args, cards, motherboard):
    env = {}
    for slot in range(3):
        option = f"sam_slot{slot}"
        value = getattr(args, option, None) or cards.get(option, "").strip()
        if not value:
            continue
        if motherboard != "later":
            raise ValueError("Internal contact modules require a later motherboard profile.")
        path = (ROOT / value).resolve()
        try:
            fixture = json.loads(path.read_text(encoding="utf-8-sig"))
        except (OSError, ValueError) as error:
            raise ValueError(f"Internal contact module {slot} fixture is missing or invalid.") from error
        if (not isinstance(fixture, dict) or fixture.get("protocol") != "async" or
                fixture.get("application") != "lab-readonly"):
            raise ValueError(f"Internal contact module {slot} requires a generic read-only async fixture.")
        try:
            atr = bytes.fromhex(fixture["atr_hex"])
            memory = bytes.fromhex(fixture.get("memory_hex", ""))
            if (not atr or len(memory) > 4096 or fixture.get("reset_policy", "external") not in ("external", "internal") or
                    any(type(fixture.get(name, default)) is not int or not lower <= fixture.get(name, default) <= upper
                        for name, default, lower, upper in (("atr_delay_us", 4000, 112, 11175),
                            ("apdu_response_delay_us", 1000, 0, 1000000), ("inter_character_delay_us", 0, 0, 100000)))
                    or fixture.get("sync_program_enabled", False)):
                raise ValueError("fixture parameters")
        except (KeyError, TypeError, ValueError) as error:
            raise ValueError(f"Internal contact module {slot} has invalid fixture parameters.") from error
        env[f"COINLINE_SAM_SLOT{slot}"] = str(path)
    return env


def data_jack_environment(args, settings, motherboard):
    profile = getattr(args, "data_jack", None) or settings.get("data_jack_profile", "disabled").strip()
    polarity = settings.get("data_jack_monitor_polarity", "").strip()
    if profile not in ("disabled", "downloaded", "desktop", "universal"):
        raise ValueError("Set a valid data_jack_profile under [hardware].")
    if polarity not in ("", "high", "low"):
        raise ValueError("Set data_jack_monitor_polarity to high, low or blank.")
    if profile == "disabled":
        return {}
    if motherboard != "later":
        raise ValueError("The selectable data-jack monitor profiles require a later motherboard.")
    env = {"COINLINE_DATA_JACK_PROFILE": profile}
    if polarity:
        env["COINLINE_DATA_JACK_MONITOR_POLARITY"] = polarity
    return env


def configure_sip(env, settings, profile):
    config_name, _, _, default_port, media_low, media_high = PROFILES[profile]
    for key, suffix in SIP_CREDENTIAL_FIELDS.items():
        value = settings.get(key, "").strip()
        if not value:
            raise ValueError(f"Set {key} under [sip] in {config_name}, or use --local for a simulated call.")
        env[f"COINLINE_SIP_{suffix}"] = value
    user = env["COINLINE_SIP_USER"].removeprefix("sip:")
    if "@" in user:
        user, domain = user.rsplit("@", 1)
        if not user or "@" in user or domain.casefold() != env["COINLINE_SIP_HOST"].casefold():
            raise ValueError("Set username to the SIP account name or its address at the configured registrar.")
    env["COINLINE_SIP_USER"] = user
    dtmf_mode = settings.get("dtmf_mode", "auto").strip().lower()
    if dtmf_mode not in ("auto", "inband", "info"):
        raise ValueError("Set dtmf_mode under [sip] to auto, inband or info.")
    env["COINLINE_SIP_DTMF_MODE"] = dtmf_mode
    port = settings.getint("registrar_port", 5060)
    if not 1 <= port <= 65535:
        raise ValueError("Set a valid SIP registrar_port.")
    env["COINLINE_SIP_PORT"] = str(port)
    local_port = settings.getint("local_port", default_port)
    if not 1 <= local_port <= 65535:
        raise ValueError("Set a valid SIP local_port.")
    env["COINLINE_SIP_LOCAL_PORT"] = str(local_port)
    env["COINLINE_SIP_RTP_PORT_LOW"] = str(media_low)
    env["COINLINE_SIP_RTP_PORT_HIGH"] = str(media_high)
    address = settings.get("local_address", "").strip()
    if not address:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
            probe.connect((env["COINLINE_SIP_HOST"], port))
            address = probe.getsockname()[0]
    env["COINLINE_SIP_MY_IP"] = address
    public_address = settings.get("rtp_public_address", "").strip()
    if public_address:
        env["COINLINE_SIP_PUBLIC_IP"] = str(IPv4Address(public_address))
    rtp_port = settings.getint("rtp_port", 0)
    if not 0 <= rtp_port <= 65535 or rtp_port == int(env["COINLINE_SIP_LOCAL_PORT"]):
        raise ValueError("Set rtp_port to 0 or a UDP port distinct from local_port.")
    if rtp_port:
        env["COINLINE_SIP_RTP_PORT"] = str(rtp_port)


def require_sip_runtime(secure):
    # The CO runs with this same Python interpreter. Detect missing wheels or
    # native extension load failures before starting the emulator or a call.
    modules = ("pyVoIP.RTP", "pyVoIP.VoIP", "pylibsrtp") if secure else ("pyVoIP.RTP", "pyVoIP.VoIP")
    try:
        for module in modules:
            importlib.import_module(module)
    except (ImportError, OSError) as error:
        remedy = "re-extract the bundled runtime" if WINDOWS else "run ./setup-sip.sh from the extracted package"
        raise ValueError(f"SIP runtime is unavailable ({module}); {remedy}.") from error


def microphone_environment(settings):
    if not settings.getboolean("capture_enabled", True):
        return {"COINLINE_MIC_DISABLED": "1"}
    if input_device := settings.get("input_device", "").strip():
        return {"COINLINE_MIC_INPUT": input_device}
    return {}


def stop_office(office):
    try:
        office.wait(timeout=4)
    except subprocess.TimeoutExpired:
        office.terminate()
        try:
            office.wait(timeout=4)
        except subprocess.TimeoutExpired:
            office.kill()
            office.wait()


def prepare_hardware(args, config):
    settings = config["hardware"]
    _, default_flash, default_display, *_ = PROFILES[args.profile]
    flash = ROOT / (args.firmware or settings.get("flash", "").strip() or default_flash)
    second = args.flash1 or (settings.get("flash1", "").strip() if args.firmware is None else "")
    program = (flash, ROOT / second) if second else (flash,)
    rompath = ROOT / (args.rompath or settings.get("rompath", "").strip() or "roms")
    display_profile = getattr(args, "display", None) or settings.get("display", default_display).strip()
    if display_profile not in ("auto", "2line", "11line"):
        raise ValueError("Set display to auto, 2line or 11line under [hardware].")
    check_files(program, rompath, "2line" if display_profile == "auto" else display_profile)
    image = program[0].read_bytes()
    if second and (len(image) != 524288 or program[1].stat().st_size != 524288):
        raise ValueError("Split flash requires two 512 KiB images in boot-chip then expansion-chip order.")
    if not second and len(image) not in (262144, 1048576):
        raise ValueError("Single flash requires a 256 KiB or 1 MiB image.")
    state = ROOT / "data" / args.profile
    mapped_image = (image + program[1].read_bytes() if second else image.ljust(1048576, b"\xff"))
    digest = hashlib.sha256(mapped_image).hexdigest()
    registry = json.loads((ROOT / "fixtures/firmware/firmware-hashes.json").read_text(encoding="utf-8"))
    entry = next((entry for entry in registry["entries"] if entry["sha256_hex"] == digest), None)
    if entry is None:
        raise ValueError(f"Register the verified mapped-flash SHA-256 {digest} in fixtures/firmware/firmware-hashes.json.")
    if display_profile == "auto":
        display_profile = entry.get("display", "2line")
        if display_profile not in ("2line", "11line"):
            raise ValueError("Invalid display profile in firmware hash registry.")
        if display_profile == "11line":
            check_files(program, rompath, display_profile)
    if args.profile == "graphical" and (display_profile != "11line" or entry.get("display") != "11line"):
        raise ValueError("The graphical launcher requires a registered 11-line firmware image.")
    preset, reason = select_hardware(args.hardware or settings.get("profile", "auto").strip(),
                                    image, entry.get("firmware_id"))
    mapping = PRESETS[preset].motherboard if preset else entry.get("motherboard", "later")
    if entry["version_id"] != "mtr19-universal" or (preset, mapping) != ("multipay-v1", "earlier"):
        state /= f"{preset or mapping}-{digest}"
    if display_profile == "11line":
        if mapping != "later":
            raise ValueError("The graphical expansion requires a later motherboard profile.")
        state /= "11line"
    env = {"COINLINE_MOTHERBOARD": mapping,
           "COINLINE_BOARD": str(ROOT / f"fixtures/board/board-profile-{display_profile}-vfd.json"),
           "COINLINE_SMART_CARD": str(ROOT / contact_fixture_default(entry))}
    if preset:
        env["COINLINE_HARDWARE_PRESET"] = preset
        env["COINLINE_CARD_DMA_CHANNEL"] = str(PRESETS[preset].card_dma_channel)
    display = args.portable_display or settings.get("portable_display", "attached").strip()
    if display not in ("attached", "detached"):
        raise ValueError("Set portable_display to attached or detached.")
    if display == "detached" and preset != "coin-basic":
        raise ValueError("Portable display removal is supported for Coin Basic.")
    env["COINLINE_PORTABLE_VFD"] = "1" if display == "attached" else "0"
    if second:
        env.update(COINLINE_FIRMWARE_FLASH0=str(program[0]), COINLINE_FIRMWARE_FLASH1=str(program[1]))
    else:
        env["COINLINE_FIRMWARE"] = str(flash)
    return state, rompath, env, f"{preset or mapping} ({reason}, {mapping} I/O, {display_profile})"


def run(args, config, hardware):
    state, rompath, hardware_env, description = hardware
    graphical = hardware_env["COINLINE_BOARD"].endswith("board-profile-11line-vfd.json")
    resolution, maximize = window_settings(args, config)
    buffer_ms, max_skip, priority = performance_settings(args, config)
    port = config["line"].getint("ncc_port", 4000)
    digits = config["line"].get("outbound_digits", "18005551212").strip()
    sip = config["sip"]
    use_sip = args.sip
    if use_sip is None:
        use_sip = any(sip.get(key, "").strip() for key in SIP_CREDENTIAL_FIELDS)
    mode = "sip" if use_sip else "local"
    if not 1 <= port <= 65535:
        raise ValueError("Set a valid ncc_port under [line] in coinline.ini.")
    code_number = config["line"].get("code_server_number", "").strip()
    code_port = config["line"].getint("code_server_port", 0)
    if code_number or code_port:
        if (len(code_number) != 10 or any(digit not in "0123456789" for digit in code_number)
                or code_number == "2130706433" or not 1 <= code_port <= 65535):
            raise ValueError("Set a distinct 10-digit code_server_number and a valid code_server_port under [line].")
    if not use_sip and (not digits or any(digit not in "0123456789*#" for digit in digits)):
        raise ValueError("Set valid outbound_digits for the local CO under [line] in coinline.ini.")
    cards = config["cards"]
    mag_card = ROOT / (args.mag_card or cards.get("mag_fixture", "").strip() or "fixtures/cards/magcard-valid.json")
    smart_card = customer_fixture_path(args, cards, hardware_env)
    if not mag_card.is_file() or not smart_card.is_file():
        raise ValueError("The selected local card fixture file is missing.")
    module_env = module_fixture_environment(args, cards, hardware_env["COINLINE_MOTHERBOARD"])
    data_jack_env = data_jack_environment(args, config["hardware"], hardware_env["COINLINE_MOTHERBOARD"])
    local_voice = adsi_route_settings(args, config)
    mic_env = microphone_environment(config["audio"])
    if use_sip or local_voice:
        require_sip_runtime(secure=use_sip)
    if args.check:
        print(f"{args.profile.upper()} package inputs are ready. Hardware: {description}")
        return 0

    run_dir = state / "logs" / datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    for path in (run_dir, state / "nvram", state / "cfg"):
        path.mkdir(parents=True, exist_ok=True)

    # Keep inherited workspace overrides from selecting a different board or battery image.
    env = {key: value for key, value in os.environ.items() if not key.upper().startswith("COINLINE_")}
    env.update({
        "COINLINE_EMU_ROOT": str(ROOT),
        **hardware_env,
        "COINLINE_TP_BACKEND": "pcd3349a",
        "COINLINE_TP8048_ROM": str(ROOT / "firmware/tp_pcd3349a_behavioural.rom"),
        "COINLINE_MAG_CARD": str(mag_card),
        "COINLINE_SMART_CARD": str(smart_card),
        **module_env,
        **data_jack_env,
        "COINLINE_LINE_PORT": get_line_port(),
        "COINLINE_NCC_PORT": str(port),
        "COINLINE_CO_MODE": mode,
        "COINLINE_CO_TIMEOUT": "0",
        "COINLINE_DIAGNOSTICS": "0",
        "PYTHONDONTWRITEBYTECODE": "1",
    })
    if graphical:
        env["COINLINE_GRAPHIC_VFD_SNAPSHOT"] = str(run_dir / "graphic-vfd.json")
    else:
        env["COINLINE_VFD_LIVE"] = str(run_dir / "display.txt")
    if use_sip:
        configure_sip(env, sip, args.profile)
    else:
        env["COINLINE_OUTBOUND"] = digits
    env.update(mic_env)
    if code_number:
        env.update(COINLINE_CODE_SERVER_NUMBER=code_number, COINLINE_CODE_SERVER_PORT=str(code_port))
    if local_voice:
        env["COINLINE_LOCAL_VOICE_ROUTE"] = json.dumps(local_voice)

    cmd = [str(ROOT / EXECUTABLE), "cl_millennium", "-noreadconfig", "-skip_gameinfo", "-nodebug", "-nolog",
           "-rompath", str(rompath), "-artpath", str(ROOT / "artwork"),
           "-nvram_directory", str(state / "nvram"), "-cfg_directory", str(state / "cfg"),
           "-snapshot_directory", str(run_dir), "-video", "none" if args.headless else args.video]
    if args.headless:
        cmd += ["-sound", "none"]
    else:
        if graphical:
            cmd += ["-view", "Graphical"]
        # Software artwork rendering must be allowed to shed display work
        # rather than slow the physical terminal or audio clock.
        cmd += ["-window", "-maximize" if maximize else "-nomaximize", "-autoframeskip", "-frameskip", str(max_skip),
                "-audio_latency", f"{buffer_ms / 20:g}"]
        if resolution is not None:
            cmd += ["-resolution", resolution]
    if args.seconds:
        cmd += ["-seconds_to_run", str(args.seconds)]
    if args.record:
        cmd += ["-wavwrite", str(run_dir / "handset.wav")]

    # A bounded scheduling preference, not high or real-time priority. Give
    # the local CO and audio/render worker threads the same Windows preference.
    scheduling_flags = subprocess.ABOVE_NORMAL_PRIORITY_CLASS if WINDOWS and priority == "above-normal" else 0
    office_flags = scheduling_flags | (subprocess.CREATE_NO_WINDOW if WINDOWS else 0)
    with (run_dir / "central-office.log").open("wb") as office_log, (run_dir / "emulator.log").open("wb") as mame_log:
        office = subprocess.Popen([sys.executable, "-u", str(ROOT / "app/central_office.py")], cwd=ROOT, env=env,
                                  stdout=office_log, stderr=subprocess.STDOUT,
                                  creationflags=office_flags)
        try:
            print(f"{args.profile.upper()} emulator, {mode.upper()} calling, hardware {description}. Run logs: {run_dir}",
                  flush=True)
            result = subprocess.run(cmd, cwd=ROOT, env=env, stdout=mame_log, stderr=subprocess.STDOUT,
                                    creationflags=scheduling_flags)
        finally:
            stop_office(office)
    if result.returncode:
        raise RuntimeError(f"MAME exited with code {result.returncode}. See {run_dir / 'emulator.log'}.")
    if b"jack C1 line present" not in (run_dir / "central-office.log").read_bytes():
        raise RuntimeError(f"The central office did not connect. See {run_dir / 'central-office.log'}.")
    return 0


def lock_session(file):
    if WINDOWS:
        import msvcrt
        msvcrt.locking(file.fileno(), msvcrt.LK_NBLCK, 1)
    else:
        import fcntl
        fcntl.flock(file.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)


def main():
    parser = argparse.ArgumentParser(description="Start the CoinLine Terminal Emulator")
    parser.add_argument("profile", choices=tuple(PROFILES))
    parser.add_argument("--hardware", choices=("auto", *PRESETS), help="select hardware by program tag or override it")
    parser.add_argument("--display", choices=("auto", "2line", "11line"), help="select the display board or use the verified registry")
    parser.add_argument("--firmware", type=Path, help="single program image or first half of split flash")
    parser.add_argument("--flash1", type=Path, help="second half of split flash")
    parser.add_argument("--rompath", type=Path, help="directory containing cl_millennium/ Voiceware images")
    parser.add_argument("--mag-card", type=Path, help="local synthetic magnetic-stripe fixture")
    parser.add_argument("--smart-card", type=Path, help="local synthetic contact-card fixture")
    for slot in range(3):
        parser.add_argument(f"--sam-slot{slot}", type=Path, help=f"generic read-only contact fixture for internal position {slot}")
    parser.add_argument("--adsi", action="store_true", help="enable the exact local InfoLine voice route")
    parser.add_argument("--data-jack", choices=("disabled", "downloaded", "desktop", "universal"),
                        help="select an optional later-board data-jack monitor/relay profile")
    parser.add_argument("--portable-display", choices=("attached", "detached"),
                        help="Coin Basic temporary display connection at power-on")
    calling_mode = parser.add_mutually_exclusive_group()
    calling_mode.add_argument("--sip", dest="sip", action="store_true", help="require the configured SIP endpoint")
    calling_mode.add_argument("--local", dest="sip", action="store_false", help="use the simulated local office")
    parser.set_defaults(sip=None)
    parser.add_argument("--check", action="store_true", help="check package inputs and exit")
    parser.add_argument("--headless", action="store_true", help="run a bounded test without a window")
    parser.add_argument("--seconds", type=int, metavar="N", help="exit after N emulated seconds")
    parser.add_argument("--scale", type=float, help="initial window size multiplier (0.5 to 4.0)")
    parser.add_argument("--maximize", action="store_true", default=None, help="start with the emulator window maximized")
    parser.add_argument("--audio-buffer-ms", type=int, help="host speaker buffer target (40 to 500 ms)")
    parser.add_argument("--max-frameskip", type=int, help="maximum automatic renderer frame skip (0 to 11)")
    parser.add_argument("--host-priority", choices=("normal", "above-normal"), help="Windows scheduling preference")
    if WINDOWS:
        parser.add_argument("--video", choices=("gdi", "d3d", "opengl"), default="opengl", help="OpenGL by default; use gdi for software rendering")
    else:
        parser.add_argument("--video", choices=("auto", "soft", "opengl", "bgfx"), default="auto")
    parser.add_argument("--record", action="store_true", help="save handset audio to the run log directory")
    args = parser.parse_args()
    if (args.seconds is not None and args.seconds <= 0) or (args.headless and not args.seconds):
        parser.error("--seconds must be positive and is required with --headless")
    try:
        config = config_for_run(args.profile)
        hardware = prepare_hardware(args, config)
        if args.check:
            return run(args, config, hardware)
        state = hardware[0]
        state.mkdir(parents=True, exist_ok=True)
        with (ROOT / "data" / args.profile / "session.lock").open("a+b") as lock:
            try:
                lock_session(lock)
            except OSError as error:
                raise RuntimeError(f"The {args.profile.upper()} profile is already running.") from error
            return run(args, config, hardware)
    except (configparser.Error, OSError, ValueError, RuntimeError) as error:
        print(f"CoinLine could not run: {error}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        return 130


if __name__ == "__main__":
    sys.exit(main())
