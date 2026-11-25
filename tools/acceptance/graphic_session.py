#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Run a real graphical firmware image with a local, physical-input command file."""
import argparse
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
    parser.add_argument("--settings", type=Path, help="local lab settings, kept out of reports")
    parser.add_argument("--battery", type=Path, help="previous run's complete nvram directory")
    parser.add_argument("--rompath", type=Path, default=ROOT / "build/package-inputs/local/roms")
    parser.add_argument("--controller", type=Path, default=ROOT / "firmware/tp_pcd3349a_behavioural.rom")
    parser.add_argument("--output", type=Path, default=ROOT / "build/firmware-acceptance/graphic-firmware")
    parser.add_argument("--seconds", type=int, default=1800)
    parser.add_argument("--open", action="store_true", help="start with the cabinet unlocked and open")
    parser.add_argument("--trace", action="store_true", help="capture the graphical I/O stream")
    parser.add_argument("--artwork", action="store_true", help="use the graphical artwork view")
    args = parser.parse_args()
    image = args.firmware.read_bytes()
    if len(image) != 1048576:
        raise ValueError("Graphical session requires a mapped 1 MiB flash image")
    digest = hashlib.sha256(image).hexdigest()
    firmware_id, revision = program_tag(image)
    preset, _ = select_hardware(args.hardware, image, firmware_id)
    if not preset or PRESETS[preset].motherboard != "later":
        raise ValueError("Select a compatible later motherboard using --hardware")
    settings = json.loads(args.settings.read_text()) if args.settings else {}
    settings = {**settings, **settings.get("firmware_overrides", {}).get(firmware_id, {})}
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
                                                "firmware_id": firmware_id, "revision": revision}, indent=2))
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
               COINLINE_BOARD=str(ROOT / "fixtures/board/board-profile-11line-vfd.json"),
               COINLINE_FIRMWARE=str(seed), COINLINE_TP_BACKEND="pcd3349a",
               COINLINE_TP8048_ROM=str(args.controller.resolve()),
               COINLINE_MAG_CARD=str(ROOT / "fixtures/cards/magcard-valid.json"),
               COINLINE_SMART_CARD=str(ROOT / "fixtures/cards/smartcard-valid.json"),
               COINLINE_LINE_PORT=port(), COINLINE_CO_MODE="local", COINLINE_CO_TIMEOUT="0",
               COINLINE_NCC_PORT=str(settings.get("ncc_port", 4000)), COINLINE_DIAGNOSTICS="0",
               COINLINE_MODEM_TRACE="1", COINLINE_BOOT_TRACE=str(out / "boot.jsonl"),
               COINLINE_GRAPHIC_COMMANDS=str(commands), COINLINE_GRAPHIC_EVENTS=str(out / "events.tsv"),
               COINLINE_GRAPHIC_OPEN="1" if args.open else "0",
               COINLINE_GRAPHIC_VFD_SNAPSHOT=str(out / "pages.json"))
    for key in ("line", "serial", "pin"):
        if key in settings:
            env["COINLINE_ACCEPTANCE_" + key.upper()] = str(settings[key])
    if settings.get("free_call"):
        env["COINLINE_OUTBOUND"] = str(settings["free_call"])
    if args.trace:
        env["COINLINE_GRAPHIC_VFD_TRACE"] = str(out / "bus.jsonl")
    cmd = [str(args.mame.resolve()), "cl_millennium", "-noreadconfig", "-skip_gameinfo", "-nodebug",
           "-nolog", "-video", "gdi" if args.artwork and os.name == "nt" else "none", "-sound", "none",
           "-rompath", str(args.rompath.resolve()), "-artpath", str(art.parent),
           "-nvram_directory", str(out / "nvram"), "-cfg_directory", str(out / "cfg"),
           "-snapshot_directory", str(out), "-snapview", "Graphical" if args.artwork else "native",
           "-view", "Graphical", "-window", "-nomaximize", "-seconds_to_run", str(args.seconds),
           "-autoboot_delay", "0", "-autoboot_script", str(Path(__file__).with_suffix(".lua").resolve())]
    with (out / "mame.log").open("wb") as log, central_office(out, env, Path(sys.executable)):
        result = subprocess.run(cmd, cwd=out, env=env, stdout=log, stderr=subprocess.STDOUT)
    print(f"Emulator exited with {result.returncode}", flush=True)
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
