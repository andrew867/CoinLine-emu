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

from run import ROOT, central_office, normalize_manager_settings, port, validate_data_routes
from hardware import COIN_VALIDATOR_PROFILES, PRESETS, coin_profile_board, program_tag, select_hardware


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mame", type=Path, required=True)
    parser.add_argument("--firmware", type=Path, required=True)
    parser.add_argument("--hardware", choices=("auto", *PRESETS), default="auto")
    parser.add_argument("--display", choices=("11line", "2line"), default="11line")
    parser.add_argument("--coin-validator", choices=tuple(COIN_VALIDATOR_PROFILES), default="board-default",
                        help="select a physical validator calibration independently of downloaded tables")
    parser.add_argument("--settings", type=Path, help="local lab settings, kept out of reports")
    parser.add_argument("--actions", type=Path, help="initial local physical-input command file, copied into the isolated run")
    parser.add_argument("--battery", type=Path, help="previous run's complete nvram directory")
    parser.add_argument("--rompath", type=Path, default=ROOT / "build/package-inputs/local/roms")
    parser.add_argument("--controller", type=Path, default=ROOT / "firmware/tp_pcd3349a_behavioural.rom")
    parser.add_argument("--mag-card", type=Path, default=ROOT / "fixtures/cards/magcard-valid.json")
    parser.add_argument("--smart-card", type=Path, default=ROOT / "fixtures/cards/smartcard-sync-gpm103-lab.json")
    parser.add_argument("--output", type=Path, default=ROOT / "build/firmware-acceptance/graphic-firmware")
    parser.add_argument("--seconds", type=int, default=1800)
    parser.add_argument("--open", action="store_true", help="start with the cabinet unlocked and open")
    parser.add_argument("--trace", action="store_true", help="capture the graphical I/O stream")
    parser.add_argument("--telephony-trace", action="store_true", help="capture startup and runtime telephony transactions")
    parser.add_argument("--keypad-trace", action="store_true", help="capture panel inputs and TP command/key transactions without per-edge CSIO logs")
    parser.add_argument("--dtmf-trace", action="store_true", help="trace controller dial-pad tone gating and genuine tone events")
    parser.add_argument("--voiceware-trace", action="store_true", help="trace native phrase/bank/reset and completion events")
    parser.add_argument("--card-trace", action="store_true", help="capture the contact-card pins")
    parser.add_argument("--coin-trace", action="store_true", help="capture the later-board validator UART register activity")
    parser.add_argument("--modem-trace", action="store_true", help="capture modem traffic without enabling default boot traces")
    parser.add_argument("--data-jack", choices=("downloaded", "desktop", "universal"), help="select an optional later-board data-jack monitor/relay profile")
    parser.add_argument("--data-jack-trace", action="store_true", help="trace raw data-jack monitor and relay transitions")
    parser.add_argument("--io-trace", action="store_true", help="capture compact native I/O trace for a bounded diagnostic run")
    parser.add_argument("--enclosure-trace", action="store_true", help="capture the native enclosure warning latch transitions")
    parser.add_argument("--sam-pin-trace", action="store_true", help="capture bounded internal contact selector and latch transitions")
    parser.add_argument("--board-supervision-trace", action="store_true", help="observe firmware watchdog pulses and telephony power indication")
    supervisor = parser.add_mutually_exclusive_group()
    supervisor.add_argument("--board-supervision-test", action="store_true", help="enable provisional later-board CPU-only watchdog and explicit power inputs")
    supervisor.add_argument("--board-power-test", action="store_true", help="enable explicit control NMI/rail inputs with no watchdog deadline")
    for slot in range(3):
        parser.add_argument(f"--sam-slot{slot}", type=Path, help=f"read-only non-payment internal contact fixture for socket {slot}")
    parser.add_argument("--quiet", action="store_true", help="omit default boot and modem traces for release diagnostics testing")
    parser.add_argument("--adsi", action="store_true", help="enable the local-only InfoLine SIP route")
    parser.add_argument("--adsi-offline-pcm", type=Path, help="deterministic signed 8 kHz line stimulus, triggered by actual configured local outpulse")
    parser.add_argument("--offline-co", action="store_true", help="retain local CO control signaling but use deterministic offline PCM")
    parser.add_argument("--sound", action="store_true", help="enable the native audio output during the bench")
    parser.add_argument("--audio-metrics", action="store_true", help="report Windows WASAPI output underruns (no audio recording)")
    parser.add_argument("--unthrottled", action="store_true", help="measure native CPU headroom; not a real-time media qualification")
    parser.add_argument("--audio-buffer-ms", type=int, default=120, help="host speaker buffer target for the timing bench")
    parser.add_argument("--max-frameskip", type=int, help="maximum automatic renderer frame skipping for the timing bench")
    parser.add_argument("--realtime-priority", action="store_true", help="use above-normal Windows process priority for timing qualification")
    parser.add_argument("--host-priority", choices=("normal", "above-normal"), default="normal", help="Windows process scheduling preference for the bench")
    parser.add_argument("--read-watch", type=Path, help="local physical-address watch list for read-only timing diagnosis")
    parser.add_argument("--read-watch-registers", action="store_true", help="include CPU register snapshots with read watches")
    parser.add_argument("--read-watch-per-frame", type=int, default=0, help="limit samples per watch address per frame (0: existing global cap only)")
    parser.add_argument("--read-watch-manual", action="store_true", help="arm read watches explicitly using watch on/off in the actions file, including on-hook activity")
    parser.add_argument("--read-watch-start", action="store_true", help="arm manual read watches from machine startup, before the first video frame")
    parser.add_argument("--cpu-sample", action="store_true", help="record bounded read-only CPU register samples at video-frame boundaries")
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
    if not 40 <= args.audio_buffer_ms <= 500:
        parser.error("--audio-buffer-ms must be between 40 and 500")
    if args.audio_metrics and (os.name != "nt" or not args.sound or args.unthrottled):
        parser.error("--audio-metrics requires a throttled Windows --sound bench")
    if args.max_frameskip is not None and not 0 <= args.max_frameskip <= 10:
        parser.error("--max-frameskip must be between 0 and 10")
    if not 0 <= args.read_watch_per_frame <= 2000:
        parser.error("--read-watch-per-frame must be between 0 and 2000")
    if (args.read_watch_registers or args.read_watch_per_frame or args.read_watch_manual or args.read_watch_start) and not args.read_watch:
        parser.error("Read-watch sampling options require --read-watch")
    if args.read_watch_start and not args.read_watch_manual:
        parser.error("--read-watch-start requires --read-watch-manual")
    if args.actions and not args.actions.is_file():
        parser.error("--actions requires an existing local command file")
    if args.data_jack_trace and not args.data_jack:
        parser.error("--data-jack-trace requires a data-jack profile")
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
    settings = normalize_manager_settings(settings)
    validate_data_routes(settings)
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
        "display": args.display, "hardware": preset, "coin_validator": args.coin_validator,
        "mag_card_sha256": hashlib.sha256(args.mag_card.read_bytes()).hexdigest(),
        "smart_card_sha256": hashlib.sha256(args.smart_card.read_bytes()).hexdigest()}, indent=2))
    if args.battery:
        shutil.copytree(args.battery, out / "nvram")
    art = out / "artwork/cl_millennium"
    art.mkdir(parents=True)
    shutil.copyfile(ROOT / "src/mame/layout/millennium.lay", art / "default.lay")
    shutil.copyfile(ROOT / "artwork/millennium-terminal-front.png", art / "millennium-terminal-front.png")
    commands = out / "commands.txt"
    if args.actions:
        shutil.copyfile(args.actions, commands)
    else:
        commands.touch()
    env = {key: value for key, value in os.environ.items() if not key.upper().startswith("COINLINE_")}
    board = coin_profile_board(ROOT / f"fixtures/board/board-profile-{args.display}-vfd.json",
                               args.coin_validator, out / "coin-validator-board.json")
    env.update(COINLINE_EMU_ROOT=str(out), COINLINE_MOTHERBOARD="later",
               COINLINE_HARDWARE_PRESET=preset, COINLINE_CARD_DMA_CHANNEL=str(PRESETS[preset].card_dma_channel),
               COINLINE_BOARD=str(board),
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
    if args.modem_trace:
        env["COINLINE_MODEM_TRACE"] = "1"
    if args.voiceware_trace:
        env["COINLINE_VOICEWARE_TRACE"] = str(out / "voiceware.jsonl")
    for key in ("line", "serial", "pin"):
        if key in settings:
            env["COINLINE_ACCEPTANCE_" + key.upper()] = str(settings[key])
    if settings.get("code_server_number"):
        env.update(COINLINE_CODE_SERVER_NUMBER=settings["code_server_number"],
                   COINLINE_CODE_SERVER_PORT=str(settings["code_server_port"]))
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
    if args.coin_trace:
        env["COINLINE_EXTERNAL_UART_TRACE"] = str(out / "external-uart.jsonl")
    if args.data_jack:
        env["COINLINE_DATA_JACK_PROFILE"] = args.data_jack
    if args.data_jack_trace:
        env["COINLINE_DATA_JACK_TRACE"] = "1"
    if args.io_trace:
        env["COINLINE_IO_TRACE"] = str(out / "io.jsonl")
    if args.enclosure_trace:
        env["COINLINE_ALERTER_TRACE"] = str(out / "enclosure-gpio.jsonl")
    if args.sam_pin_trace:
        env["COINLINE_SAM_PIN_TRACE"] = "1"
    if args.board_supervision_trace or args.board_supervision_test or args.board_power_test:
        env["COINLINE_BOARD_SUPERVISION_TRACE"] = "1"
    if args.board_supervision_test:
        env["COINLINE_BOARD_SUPERVISION_PROFILE"] = "cp-only"
    elif args.board_power_test:
        env["COINLINE_BOARD_SUPERVISION_PROFILE"] = "cp-interrupt"
    for slot, path in sam_slots.items():
        env[f"COINLINE_SAM_SLOT{slot}"] = str(path.resolve())
    if sam_slots:
        (out / "sam-lab-fixtures.json").write_text(json.dumps({str(slot): hashlib.sha256(path.read_bytes()).hexdigest()
            for slot, path in sam_slots.items()}, indent=2), encoding="utf-8")
    if args.dtmf_trace:
        env["COINLINE_DTMF_TRACE"] = "1"
    if args.cpu_sample:
        env["COINLINE_GRAPHIC_CPU_SAMPLE"] = str(out / "cpu-sample.tsv")
    if args.read_watch:
        if not args.read_watch.is_file():
            parser.error("--read-watch must be a local file")
        env.update(COINLINE_GRAPHIC_READ_WATCH=str(args.read_watch.resolve()),
                   COINLINE_GRAPHIC_READ_WATCH_OUT=str(out / "read-watch.tsv"))
        if args.read_watch_registers:
            env["COINLINE_GRAPHIC_READ_WATCH_REGISTERS"] = "1"
        if args.read_watch_per_frame:
            env["COINLINE_GRAPHIC_READ_WATCH_PER_FRAME"] = str(args.read_watch_per_frame)
        if args.read_watch_manual:
            env["COINLINE_GRAPHIC_READ_WATCH_MANUAL"] = "1"
        if args.read_watch_start:
            env["COINLINE_GRAPHIC_READ_WATCH_START"] = "1"
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
           "-nolog", "-video", args.video if args.artwork else "none", "-sound", "wasapi" if args.audio_metrics else "auto" if args.sound else "none",
           "-rompath", str(args.rompath.resolve()), "-artpath", str(art.parent),
           "-nvram_directory", str(out / "nvram"), "-cfg_directory", str(out / "cfg"),
           "-snapshot_directory", str(out), "-snapview", view if args.artwork and not args.snapshot_native else "native",
           "-view", view, "-window", "-nomaximize", "-seconds_to_run", str(args.seconds),
           "-autoboot_delay", "0", "-autoboot_script", str(Path(__file__).with_suffix(".lua").resolve())]
    if args.realtime_priority and os.name == "nt":
        cmd.extend(["-priority", "1"])
    if args.unthrottled:
        cmd.append("-nothrottle")
    if args.audio_metrics:
        cmd.append("-verbose")
    if args.sound:
        cmd.extend(["-audio_latency", str(args.audio_buffer_ms / 20)])
    if args.artwork:
        max_skip = args.max_frameskip if args.max_frameskip is not None else (10 if args.video == "gdi" else 4)
        cmd.extend(["-autoframeskip", "-frameskip", str(max_skip)])
        if os.name == "nt":
            cmd.extend(["-resolution", "360x850"])
    office = nullcontext() if args.external_co_port or (args.adsi_offline_pcm and not args.offline_co) else central_office(out, env, Path(sys.executable))
    with (out / "mame.log").open("wb") as log, office:
        flags = subprocess.ABOVE_NORMAL_PRIORITY_CLASS if os.name == "nt" and args.host_priority == "above-normal" else 0
        result = subprocess.run(cmd, cwd=out, env=env, stdout=log, stderr=subprocess.STDOUT, creationflags=flags)
    print(f"Emulator exited with {result.returncode}", flush=True)
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
