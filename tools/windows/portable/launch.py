# SPDX-License-Identifier: GPL-2.0-or-later
import argparse
import configparser
from datetime import datetime, timezone
import hashlib
from ipaddress import IPv4Address
import json
import os
from pathlib import Path
import socket
import subprocess
import sys

from hardware import PRESETS, select_hardware


ROOT = Path(__file__).resolve().parent.parent
WINDOWS = os.name == "nt"
EXECUTABLE = "bin/coinline-mame.exe" if WINDOWS else "bin/coinline-mame"
FIRMWARE = "firmware/oem/flash.bin"
SIP_CREDENTIAL_FIELDS = {"registrar": "HOST", "username": "USER", "password": "PASSWORD"}


def config_for_run():
    config = configparser.ConfigParser(interpolation=None)
    config.read(ROOT / "coinline.ini", encoding="utf-8-sig")
    for section in ("line", "sip", "hardware", "cards", "adsi"):
        if section not in config:
            config.add_section(section)
    return config


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


def configure_sip(env, settings):
    for key, suffix in SIP_CREDENTIAL_FIELDS.items():
        value = settings.get(key, "").strip()
        if not value:
            raise ValueError(f"Set {key} under [sip] in coinline.ini, or use --local for a simulated call.")
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
    env["COINLINE_SIP_PORT"] = str(port)
    env["COINLINE_SIP_LOCAL_PORT"] = str(settings.getint("local_port", 5087))
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


def stop_office(office):
    try:
        office.wait(timeout=4)
    except subprocess.TimeoutExpired:
        office.terminate()
        office.wait()


def prepare_hardware(args, config):
    settings = config["hardware"]
    flash = ROOT / (args.firmware or settings.get("flash", "").strip() or FIRMWARE)
    second = args.flash1 or (settings.get("flash1", "").strip() if args.firmware is None else "")
    program = (flash, ROOT / second) if second else (flash,)
    rompath = ROOT / (args.rompath or settings.get("rompath", "").strip() or "roms")
    display_profile = getattr(args, "display", None) or settings.get("display", "auto").strip()
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
           "COINLINE_BOARD": str(ROOT / f"fixtures/board/board-profile-{display_profile}-vfd.json")}
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
    port = config["line"].getint("ncc_port", 4000)
    digits = config["line"].get("outbound_digits", "18005551212").strip()
    sip = config["sip"]
    use_sip = args.sip
    if use_sip is None:
        use_sip = any(sip.get(key, "").strip() for key in SIP_CREDENTIAL_FIELDS)
    if use_sip and not WINDOWS:
        raise ValueError("SIP audio requires the Windows build. Use --local for the simulated office.")
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
    smart_card = ROOT / (args.smart_card or cards.get("smart_fixture", "").strip() or "fixtures/cards/smartcard-sync-test.json")
    if not mag_card.is_file() or not smart_card.is_file():
        raise ValueError("The selected local card fixture file is missing.")
    adsi = config["adsi"]
    local_voice = None
    if args.adsi or adsi.getboolean("enabled", fallback=False):
        aliases = [value.strip() for value in adsi.get("dial_digits", "2374,7095550174,17095550174").split(",")]
        target = adsi.get("target", "2374").strip()
        voice_port = adsi.getint("port", fallback=5069)
        if (not aliases or any(not value or len(value) > 15 or not value.isascii() or not value.isdigit()
                               or value == "2130706433" for value in aliases)
                or len(set(aliases)) != len(aliases) or not target or len(target) > 15
                or not target.isascii() or not target.isdigit() or not 1 <= voice_port <= 65535):
            raise ValueError("Set valid exact dial_digits, target and local UDP port under [adsi].")
        local_voice = {"digits": aliases, "target": target, "port": voice_port}
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
        configure_sip(env, sip)
    else:
        env["COINLINE_OUTBOUND"] = digits
    if input_device := config.get("audio", "input_device", fallback="").strip():
        env["COINLINE_MIC_INPUT"] = input_device
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
        cmd += ["-window", "-nomaximize", "-autoframeskip", "-frameskip", "11" if args.video == "gdi" else "4", "-audio_latency", "6"]
        if WINDOWS:
            cmd += ["-resolution", "360x850"]
    if args.seconds:
        cmd += ["-seconds_to_run", str(args.seconds)]
    if args.record:
        cmd += ["-wavwrite", str(run_dir / "handset.wav")]

    with (run_dir / "central-office.log").open("wb") as office_log, (run_dir / "emulator.log").open("wb") as mame_log:
        office = subprocess.Popen([sys.executable, "-u", str(ROOT / "app/central_office.py")], cwd=ROOT, env=env,
                                  stdout=office_log, stderr=subprocess.STDOUT,
                                  creationflags=subprocess.CREATE_NO_WINDOW if WINDOWS else 0)
        try:
            print(f"{args.profile.upper()} emulator, {mode.upper()} calling, hardware {description}. Run logs: {run_dir}",
                  flush=True)
            result = subprocess.run(cmd, cwd=ROOT, env=env, stdout=mame_log, stderr=subprocess.STDOUT)
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
    parser.add_argument("profile", choices=("oem",))
    parser.add_argument("--hardware", choices=("auto", *PRESETS), help="select hardware by program tag or override it")
    parser.add_argument("--display", choices=("auto", "2line", "11line"), help="select the display board or use the verified registry")
    parser.add_argument("--firmware", type=Path, help="single program image or first half of split flash")
    parser.add_argument("--flash1", type=Path, help="second half of split flash")
    parser.add_argument("--rompath", type=Path, help="directory containing cl_millennium/ Voiceware images")
    parser.add_argument("--mag-card", type=Path, help="local synthetic magnetic-stripe fixture")
    parser.add_argument("--smart-card", type=Path, help="local synthetic contact-card fixture")
    parser.add_argument("--adsi", action="store_true", help="enable the exact local InfoLine voice route")
    parser.add_argument("--portable-display", choices=("attached", "detached"),
                        help="Coin Basic temporary display connection at power-on")
    calling_mode = parser.add_mutually_exclusive_group()
    calling_mode.add_argument("--sip", dest="sip", action="store_true", help="require the configured SIP endpoint")
    calling_mode.add_argument("--local", dest="sip", action="store_false", help="use the simulated local office")
    parser.set_defaults(sip=None)
    parser.add_argument("--check", action="store_true", help="check package inputs and exit")
    parser.add_argument("--headless", action="store_true", help="run a bounded test without a window")
    parser.add_argument("--seconds", type=int, metavar="N", help="exit after N emulated seconds")
    if WINDOWS:
        parser.add_argument("--video", choices=("gdi", "d3d", "opengl"), default="opengl", help="OpenGL by default; use gdi for software rendering")
    else:
        parser.add_argument("--video", choices=("auto", "soft", "opengl", "bgfx"), default="auto")
    parser.add_argument("--record", action="store_true", help="save handset audio to the run log directory")
    args = parser.parse_args()
    if (args.seconds is not None and args.seconds <= 0) or (args.headless and not args.seconds):
        parser.error("--seconds must be positive and is required with --headless")
    try:
        config = config_for_run()
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
