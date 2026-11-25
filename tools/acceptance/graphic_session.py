#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Run a real later-board firmware image with a local, physical-input command file."""
import argparse
from contextlib import nullcontext
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

from run import ROOT, central_office, port
from hardware import PRESETS, program_tag, select_hardware


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mame", type=Path, required=True)
    parser.add_argument("--firmware", type=Path, required=True)
    parser.add_argument("--hardware", choices=("auto", *PRESETS), default="auto")
    parser.add_argument("--display", choices=("11line", "2line"), default="11line")
    parser.add_argument("--settings", type=Path, help="local lab settings, kept out of reports")
    parser.add_argument("--battery", type=Path, help="previous run's complete nvram directory")
    parser.add_argument("--rompath", type=Path, default=ROOT / "build/package-inputs/local/roms")
    parser.add_argument("--controller", type=Path, default=ROOT / "firmware/tp_pcd3349a_behavioural.rom")
    parser.add_argument("--mag-card", type=Path, default=ROOT / "fixtures/cards/magcard-valid.json")
    parser.add_argument("--smart-card", type=Path, default=ROOT / "fixtures/cards/smartcard-sync-test.json")
    parser.add_argument("--output", type=Path, default=ROOT / "build/firmware-acceptance/graphic-firmware")
    parser.add_argument("--seconds", type=int, default=1800)
    parser.add_argument("--open", action="store_true", help="start with the cabinet unlocked and open")
    parser.add_argument("--trace", action="store_true", help="capture the graphical I/O stream")
    parser.add_argument("--telephony-trace", action="store_true", help="capture startup and runtime telephony transactions")
    parser.add_argument("--keypad-trace", action="store_true", help="capture panel inputs and TP command/key transactions without per-edge CSIO logs")
    parser.add_argument("--dtmf-trace", action="store_true", help="trace controller dial-pad tone gating and genuine tone events")
    parser.add_argument("--card-trace", action="store_true", help="capture the contact-card pins")
    parser.add_argument("--sam-pin-trace", action="store_true", help="capture bounded internal contact selector and latch transitions")
    for slot in range(3):
        parser.add_argument(f"--sam-slot{slot}", type=Path, help=f"read-only non-payment internal contact fixture for socket {slot}")
    parser.add_argument("--quiet", action="store_true", help="omit default boot and modem traces for release diagnostics testing")
    parser.add_argument("--adsi", action="store_true", help="enable the local-only InfoLine SIP route")
    parser.add_argument("--adsi-offline-pcm", type=Path, help="deterministic signed 8 kHz line stimulus, triggered by actual configured local outpulse")
    parser.add_argument("--offline-co", action="store_true", help="retain local CO control signaling but use deterministic offline PCM")
    parser.add_argument("--sound", action="store_true", help="enable the native audio output during the bench")
    parser.add_argument("--realtime-priority", action="store_true", help="use above-normal Windows process priority for timing qualification")
    parser.add_argument("--read-watch", type=Path, help="local physical-address watch list for read-only timing diagnosis")
    parser.add_argument("--artwork", action="store_true", help="use the graphical artwork view")
    parser.add_argument("--video", choices=("gdi", "d3d", "opengl") if os.name == "nt" else ("auto", "soft", "opengl", "bgfx"),
                        default="opengl" if os.name == "nt" else "auto", help="renderer when artwork is enabled")
    parser.add_argument("--external-co-port", type=int, help="fixed local jack port; an independently managed CO connects to it")
    parser.add_argument("--snapshot-interval", type=int, default=180, metavar="FRAMES",
                        help="automatic native screenshot interval; zero disables it for timing tests")
    parser.add_argument("--snapshot-native", action="store_true", help="capture the native VFD rather than the complete artwork")
    args = parser.parse_args()
    if args.external_co_port is not None and not 1 <= args.external_co_port <= 65535:
        parser.error("--external-co-port must be a valid TCP port")
    if args.external_co_port and args.adsi_offline_pcm:
        parser.error("Use the dedicated offline CO controls for deterministic PCM tests")
    if args.snapshot_interval < 0:
        parser.error("--snapshot-interval cannot be negative")
    sam_slots = {slot: getattr(args, f"sam_slot{slot}") for slot in range(3) if getattr(args, f"sam_slot{slot}") is not None}
    if any(not path.is_file() for path in sam_slots.values()):
        parser.error("Each configured internal contact fixture must be an existing local file.")
    if args.adsi_offline_pcm and (args.adsi or args.display != "11line" or not args.adsi_offline_pcm.is_file()):
        parser.error("--adsi-offline-pcm requires an existing fixture, 11line display and no SIP route")
    if args.offline_co and not args.adsi_offline_pcm:
        parser.error("--offline-co requires --adsi-offline-pcm")
    image = args.firmware.read_bytes()
    if len(image) != 1048576:
        raise ValueError("Graphical session requires a mapped 1 MiB flash image")
    digest = hashlib.sha256(image).hexdigest()
    firmware_id, tag_version = program_tag(image)
    preset, _ = select_hardware(args.hardware, image, firmware_id)
    if not preset or PRESETS[preset].motherboard != "later":
        raise ValueError("Select a compatible later motherboard using --hardware")
    settings = json.loads(args.settings.read_text()) if args.settings else {}
    settings = {**settings, **settings.get("firmware_overrides", {}).get(firmware_id, {})}
    if args.adsi_offline_pcm and not str(settings.get("free_call", "")).isdigit():
        parser.error("--adsi-offline-pcm requires a numeric local free_call in the lab settings")
    if "line" not in settings and "wireId" in settings:
        settings["line"] = settings["wireId"]
    if "ncc_port" not in settings and "nccPort" in settings:
        settings["ncc_port"] = settings["nccPort"]
    out = args.output.resolve() / datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    out.mkdir(parents=True)
    print(out, flush=True)
    shutil.copytree(ROOT / "fixtures", out / "fixtures")
    seed = out / "flash.bin"
    seed.write_bytes(image)
    entry = {"version_id": "local-graphic-firmware", "firmware_id": firmware_id,
             "friendly_name": "Local graphical firmware", "sha256_hex": digest,
             "size_bytes": len(image), "motherboard": "later"}
    (out / "fixtures/firmware/firmware-hashes.json").write_text(json.dumps({"entries": [entry]}))
    (out / "input.json").write_text(json.dumps({"sha256": digest, "size": len(image),
        "firmware_id": firmware_id, "program_tag_version": tag_version,
        "display": args.display, "hardware": preset,
        "mag_card_sha256": hashlib.sha256(args.mag_card.read_bytes()).hexdigest(),
        "smart_card_sha256": hashlib.sha256(args.smart_card.read_bytes()).hexdigest()}, indent=2))
    if args.battery:
        shutil.copytree(args.battery, out / "nvram")
    art = out / "artwork/cl_millennium"
    art.mkdir(parents=True)
    shutil.copyfile(ROOT / "src/mame/layout/millennium.lay", art / "default.lay")
    shutil.copyfile(ROOT / "artwork/millennium-terminal-front.png", art / "millennium-terminal-front.png")
    commands = out / "commands.txt"
    commands.touch()
    env = {key: value for key, value in os.environ.items() if not key.upper().startswith("COINLINE_")}
    env.update(COINLINE_EMU_ROOT=str(out), COINLINE_MOTHERBOARD="later",
               COINLINE_HARDWARE_PRESET=preset, COINLINE_CARD_DMA_CHANNEL=str(PRESETS[preset].card_dma_channel),
               COINLINE_BOARD=str(ROOT / f"fixtures/board/board-profile-{args.display}-vfd.json"),
               COINLINE_FIRMWARE=str(seed), COINLINE_TP_BACKEND="pcd3349a",
               COINLINE_TP8048_ROM=str(args.controller.resolve()),
               COINLINE_MAG_CARD=str(args.mag_card.resolve()),
               COINLINE_SMART_CARD=str(args.smart_card.resolve()),
               COINLINE_LINE_PORT=str(args.external_co_port) if args.external_co_port else port(), COINLINE_CO_MODE="local", COINLINE_CO_TIMEOUT="0",
               COINLINE_NCC_PORT=str(settings.get("ncc_port", 4000)), COINLINE_DIAGNOSTICS="0",
               COINLINE_GRAPHIC_COMMANDS=str(commands), COINLINE_GRAPHIC_EVENTS=str(out / "events.tsv"),
               COINLINE_GRAPHIC_OPEN="1" if args.open else "0", COINLINE_GRAPHIC_SNAPSHOT_INTERVAL=str(args.snapshot_interval))
    if args.display == "11line":
        env["COINLINE_GRAPHIC_VFD_SNAPSHOT"] = str(out / "pages.json")
    else:
        env["COINLINE_VFD_LIVE"] = str(out / "display.txt")
    if not args.quiet:
        env.update(COINLINE_MODEM_TRACE="1", COINLINE_BOOT_TRACE=str(out / "boot.jsonl"))
    for key in ("line", "serial", "pin"):
        if key in settings:
            env["COINLINE_ACCEPTANCE_" + key.upper()] = str(settings[key])
    if settings.get("free_call"):
        env["COINLINE_OUTBOUND"] = str(settings["free_call"])
    if args.adsi:
        env["COINLINE_LOCAL_VOICE_ROUTE"] = json.dumps(settings.get("adsi_route", {
            "digits": ["2374", "7095550174", "17095550174"], "target": "2374", "port": 5069}))
        env["COINLINE_ADSI_TRACE"] = "1"
    if args.adsi_offline_pcm:
        env.update(COINLINE_ADSI_PCM_FIXTURE=str(args.adsi_offline_pcm.resolve()), COINLINE_ADSI_TRACE="1")
        if not args.offline_co:
            env.update(COINLINE_LINE_PORT="0", COINLINE_GRAPHIC_OFFLINE_LINE="1")
        report_path = out / "offline-pcm.json"
        raw = args.adsi_offline_pcm.read_bytes()
        report_path.write_text(json.dumps({"sha256": hashlib.sha256(raw).hexdigest(), "sample_rate": 8000,
                                           "samples": len(raw) // 2}, indent=2))
    if args.trace and args.display == "11line":
        env["COINLINE_GRAPHIC_VFD_TRACE"] = str(out / "bus.jsonl")
    if args.card_trace:
        env["COINLINE_SMART_CARD_TRACE"] = "1"
    if args.sam_pin_trace:
        env["COINLINE_SAM_PIN_TRACE"] = "1"
    for slot, path in sam_slots.items():
        env[f"COINLINE_SAM_SLOT{slot}"] = str(path.resolve())
    if sam_slots:
        (out / "sam-lab-fixtures.json").write_text(json.dumps({str(slot): hashlib.sha256(path.read_bytes()).hexdigest()
            for slot, path in sam_slots.items()}, indent=2), encoding="utf-8")
    if args.dtmf_trace:
        env["COINLINE_DTMF_TRACE"] = "1"
    if args.read_watch:
        if not args.read_watch.is_file():
            parser.error("--read-watch must be a local file")
        env.update(COINLINE_GRAPHIC_READ_WATCH=str(args.read_watch.resolve()),
                   COINLINE_GRAPHIC_READ_WATCH_OUT=str(out / "read-watch.tsv"))
    if args.telephony_trace:
        env.update(COINLINE_TP_CSIO_RAW_TRACE=str(out / "csio-wire.jsonl"),
                   COINLINE_TP_CSIO_QUALIFIED_TRACE=str(out / "csio-qualified.jsonl"),
                   COINLINE_TP_8048_CP_PROTOCOL_TRACE=str(out / "telephony.jsonl"),
                   COINLINE_TP_RUNTIME_HEALTH_TRACE=str(out / "telephony-health.jsonl"))
    if args.keypad_trace:
        env.update(COINLINE_FRONT_PANEL_TRACE=str(out / "front-panel.jsonl"),
                   COINLINE_HW_CNTL_RELAY_TRACE=str(out / "hw-cntl-relay-trace.jsonl"),
                   COINLINE_TP_8048_CP_PROTOCOL_TRACE=str(out / "telephony.jsonl"),
                   COINLINE_TP_8048_KEYPAD_TRACE=str(out / "keypad.jsonl"),
                   COINLINE_TP_8048_RUNTIME_TRACE=str(out / "telephony-clock.jsonl"),
                   COINLINE_TP_RUNTIME_HEALTH_TRACE=str(out / "telephony-health.jsonl"))
    view = "Graphical" if args.display == "11line" else "Standard"
    cmd = [str(args.mame.resolve()), "cl_millennium", "-noreadconfig", "-skip_gameinfo", "-nodebug",
           "-nolog", "-video", args.video if args.artwork else "none", "-sound", "auto" if args.sound else "none",
           "-rompath", str(args.rompath.resolve()), "-artpath", str(art.parent),
           "-nvram_directory", str(out / "nvram"), "-cfg_directory", str(out / "cfg"),
           "-snapshot_directory", str(out), "-snapview", view if args.artwork and not args.snapshot_native else "native",
           "-view", view, "-window", "-nomaximize", "-seconds_to_run", str(args.seconds),
           "-autoboot_delay", "0", "-autoboot_script", str(Path(__file__).with_suffix(".lua").resolve())]
    if args.realtime_priority and os.name == "nt":
        cmd.extend(["-priority", "1"])
    if args.artwork:
        cmd.extend(["-autoframeskip", "-frameskip", "11" if args.video == "gdi" else "4", "-audio_latency", "6"])
        if os.name == "nt":
            cmd.extend(["-resolution", "360x850"])
    office = nullcontext() if args.external_co_port or (args.adsi_offline_pcm and not args.offline_co) else central_office(out, env, Path(sys.executable))
    with (out / "mame.log").open("wb") as log, office:
        result = subprocess.run(cmd, cwd=out, env=env, stdout=log, stderr=subprocess.STDOUT)
    print(f"Emulator exited with {result.returncode}", flush=True)
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
