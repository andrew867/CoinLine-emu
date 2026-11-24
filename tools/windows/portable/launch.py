# SPDX-License-Identifier: GPL-2.0-or-later
import argparse
import configparser
from datetime import datetime, timezone
from ipaddress import IPv4Address
import os
from pathlib import Path
import socket
import subprocess
import sys


ROOT = Path(__file__).resolve().parent.parent
WINDOWS = os.name == "nt"
EXECUTABLE = "bin/coinline-mame.exe" if WINDOWS else "bin/coinline-mame"
FIRMWARE = "firmware/oem/flash.bin"
SIP_CREDENTIAL_FIELDS = {"registrar": "HOST", "username": "USER", "password": "PASSWORD"}


def config_for_run():
    config = configparser.ConfigParser(interpolation=None)
    config.read(ROOT / "coinline.ini", encoding="utf-8-sig")
    for section in ("line", "sip"):
        if section not in config:
            config.add_section(section)
    return config


def check_files():
    files = [EXECUTABLE, "firmware/tp_pcd3349a_behavioural.rom",
             "roms/cl_millennium/voice_u16.bin", "roms/cl_millennium/voice_u26.bin",
             "fixtures/board/board-profile-2line-vfd.json", "fixtures/firmware/firmware-hashes.json",
             "fixtures/cards/magcard-valid.json", "app/central_office.py",
             "app/sip_media.py",
             FIRMWARE]
    missing = [name for name in files if not (ROOT / name).is_file()]
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


def run(args):
    check_files()
    config = config_for_run()
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
    if not use_sip and (not digits or any(digit not in "0123456789*#" for digit in digits)):
        raise ValueError("Set valid outbound_digits for the local CO under [line] in coinline.ini.")
    if args.check:
        print(f"{args.profile.upper()} package inputs are ready.")
        return 0

    state = ROOT / "data" / args.profile
    run_dir = state / "logs" / datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    for path in (run_dir, state / "nvram", state / "cfg"):
        path.mkdir(parents=True, exist_ok=True)

    # Keep inherited workspace overrides from selecting a different board or battery image.
    env = {key: value for key, value in os.environ.items() if not key.upper().startswith("COINLINE_")}
    env.update({
        "COINLINE_EMU_ROOT": str(ROOT),
        "COINLINE_FIRMWARE": str(ROOT / FIRMWARE),
        "COINLINE_MOTHERBOARD": "earlier",
        "COINLINE_TP_BACKEND": "pcd3349a",
        "COINLINE_TP8048_ROM": str(ROOT / "firmware/tp_pcd3349a_behavioural.rom"),
        "COINLINE_MAG_CARD": str(ROOT / "fixtures/cards/magcard-valid.json"),
        "COINLINE_LINE_PORT": get_line_port(),
        "COINLINE_NCC_PORT": str(port),
        "COINLINE_CO_MODE": mode,
        "COINLINE_CO_TIMEOUT": "0",
        "COINLINE_VFD_LIVE": str(run_dir / "display.txt"),
        "COINLINE_DIAGNOSTICS": "0",
        "PYTHONDONTWRITEBYTECODE": "1",
    })
    if use_sip:
        configure_sip(env, sip)
    else:
        env["COINLINE_OUTBOUND"] = digits
    if input_device := config.get("audio", "input_device", fallback="").strip():
        env["COINLINE_MIC_INPUT"] = input_device

    cmd = [str(ROOT / EXECUTABLE), "cl_millennium", "-noreadconfig", "-skip_gameinfo", "-nodebug", "-nolog",
           "-rompath", str(ROOT / "roms"), "-artpath", str(ROOT / "artwork"),
           "-nvram_directory", str(state / "nvram"), "-cfg_directory", str(state / "cfg"),
           "-snapshot_directory", str(run_dir), "-video", "none" if args.headless else args.video]
    if args.headless:
        cmd += ["-sound", "none"]
    else:
        cmd += ["-window", "-nomaximize", "-autoframeskip", "-frameskip", "4", "-audio_latency", "6"]
        if args.video == "gdi":
            cmd += ["-resolution", "360x850"]
    if args.seconds:
        cmd += ["-seconds_to_run", str(args.seconds)]
    if args.record:
        cmd += ["-wavwrite", str(run_dir / "handset.wav")]

    with (run_dir / "central-office.log").open("wb") as office_log, (run_dir / "emulator.log").open("wb") as mame_log:
        office = subprocess.Popen([sys.executable, "-u", str(ROOT / "app/central_office.py")], cwd=ROOT, env=env,
                                  stdout=office_log, stderr=subprocess.STDOUT,
                                  creationflags=subprocess.CREATE_NO_WINDOW)
        try:
            print(f"{args.profile.upper()} emulator, {mode.upper()} calling. Run logs: {run_dir}", flush=True)
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
    calling_mode = parser.add_mutually_exclusive_group()
    calling_mode.add_argument("--sip", dest="sip", action="store_true", help="require the configured SIP endpoint")
    calling_mode.add_argument("--local", dest="sip", action="store_false", help="use the simulated local office")
    parser.set_defaults(sip=None)
    parser.add_argument("--check", action="store_true", help="check package inputs and exit")
    parser.add_argument("--headless", action="store_true", help="run a bounded test without a window")
    parser.add_argument("--seconds", type=int, metavar="N", help="exit after N emulated seconds")
    if WINDOWS:
        parser.add_argument("--video", choices=("gdi", "d3d", "opengl"), default="gdi")
    else:
        parser.add_argument("--video", choices=("auto", "soft", "opengl", "bgfx"), default="auto")
    parser.add_argument("--record", action="store_true", help="save handset audio to the run log directory")
    args = parser.parse_args()
    if (args.seconds is not None and args.seconds <= 0) or (args.headless and not args.seconds):
        parser.error("--seconds must be positive and is required with --headless")
    try:
        if args.check:
            return run(args)
        state = ROOT / "data" / args.profile
        state.mkdir(parents=True, exist_ok=True)
        with (state / "session.lock").open("a+b") as lock:
            try:
                lock_session(lock)
            except OSError as error:
                raise RuntimeError(f"The {args.profile.upper()} profile is already running.") from error
            return run(args)
    except (configparser.Error, OSError, ValueError, RuntimeError) as error:
        print(f"CoinLine could not run: {error}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        return 130


if __name__ == "__main__":
    sys.exit(main())
