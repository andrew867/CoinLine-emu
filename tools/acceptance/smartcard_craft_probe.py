#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Exercise an installed terminal's genuine Craft card-contact test."""

import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

from connected_dtmf_native_probe import ROOT, port, wait_for


def contact_evidence(log, fixture):
    # Only the bundled non-payment contact fixture is used by this probe.
    memory = bytes.fromhex(json.loads(fixture.read_text())["memory_hex"])
    reads = [(int(bit), int(value)) for bit, value in re.findall(
        r"smart-card read \d+ bit=(\d+) value=([01])", log.read_text(errors="replace"))]
    mismatches = sum(value != ((memory[bit // 8] >> (7 - bit % 8)) & 1 if bit < len(memory)*8 else 1)
                     for bit, value in reads)
    return {"contact_read_count": len(reads), "contact_distinct_addresses": len({bit for bit, _ in reads}),
            "contact_mismatches": mismatches, "contact_data_matches": bool(reads) and mismatches == 0}


def craft_actions(startup_frames, item):
    return [f"wait {startup_frames}", "snapshot before-craft", "cabinet open", "wait 60",
            "pin", "keyseq *", "wait 180", "snapshot craft-menu", f"keyseq {item}", "wait 180",
            "snapshot card-test-menu", "keyseq 1", "wait 180", "snapshot insert-prompt", "smart",
            "wait 360", "snapshot card-test-result", "smart", "keyseq *", "wait 90",
            "cabinet close", "wait 360", "snapshot returned-service", "exit"]


def probe_actions(args):
    if args.runtime_contact:
        return [f"wait {args.startup_frames}", "snapshot before-contact", "hook off", "wait 120", "smart",
                "wait 360", "snapshot card-test-result", "smart", "hook on", "wait 360",
                "snapshot returned-service", "exit"]
    return craft_actions(args.startup_frames, args.item)


def run_oem_package(args, out, settings, env):
    package = args.package.resolve()
    for name in ("firmware/oem/flash.bin", "firmware/tp_pcd3349a_behavioural.rom",
                 "roms/cl_millennium/voice_u16.bin", "roms/cl_millennium/voice_u26.bin"):
        if not (package / name).is_file():
            raise ValueError(f"Extracted OEM package is missing {name}")
    run = out / "terminal" / "oem"
    run.mkdir(parents=True)
    shutil.copytree(args.battery, run / "nvram")
    commands = run / "commands.txt"
    commands.write_text("\n".join(probe_actions(args)) + "\n", encoding="ascii")
    env.update(COINLINE_EMU_ROOT=str(package), COINLINE_MOTHERBOARD="earlier", COINLINE_HARDWARE_PRESET="multipay-v1",
               COINLINE_CARD_DMA_CHANNEL="0", COINLINE_DIAGNOSTICS="0", COINLINE_SMART_CARD_TRACE="1",
               COINLINE_BOARD=str(package / "fixtures/board/board-profile-2line-vfd.json"),
               COINLINE_TP8048_ROM=str(package / "firmware/tp_pcd3349a_behavioural.rom"),
               COINLINE_FIRMWARE=str(package / "firmware/oem/flash.bin"),
               COINLINE_SMART_CARD=str(package / "fixtures/cards/smartcard-sync-test.json"),
               COINLINE_GRAPHIC_COMMANDS=str(commands), COINLINE_GRAPHIC_EVENTS=str(run / "events.tsv"),
               COINLINE_GRAPHIC_SNAPSHOT_INTERVAL="0", COINLINE_VFD_LIVE=str(run / "display.txt"),
               COINLINE_ACCEPTANCE_PIN=str(settings.get("pin", "")), COINLINE_NCC_PORT=str(settings["ncc_port"]),
               COINLINE_LINE_PORT=port(), COINLINE_CO_MODE="local", COINLINE_CO_TIMEOUT="0")
    command = [str(args.mame.resolve()), "cl_millennium", "-noreadconfig", "-skip_gameinfo", "-nodebug", "-nolog",
               "-video", "none", "-sound", "none", "-rompath", str(package / "roms"),
               "-artpath", str(package / "artwork"), "-nvram_directory", str(run / "nvram"),
               "-cfg_directory", str(run / "cfg"), "-snapshot_directory", str(run), "-snapview", "native",
               "-seconds_to_run", "240", "-autoboot_delay", "0", "-autoboot_script",
               str(ROOT / "tools/acceptance/graphic_session.lua")]
    with (run / "co.log").open("wb") as co_log, (run / "mame.log").open("wb") as native_log:
        office = subprocess.Popen([sys.executable, "-u", str(ROOT / "tools/line-jack/central_office.py")],
                                  cwd=run, env=env, stdout=co_log, stderr=subprocess.STDOUT)
        try:
            native = subprocess.run(command, cwd=run, env=env, stdout=native_log, stderr=subprocess.STDOUT, timeout=300)
        finally:
            try:
                office.wait(timeout=5)
            except subprocess.TimeoutExpired:
                office.terminate()
                office.wait(timeout=5)
    return {"native_exit_code": native.returncode, "display_review_required": True, "run": run.name,
            **contact_evidence(run / "mame.log", package / "fixtures/cards/smartcard-sync-test.json"),
            "result_snapshot_present": (run / "card-test-result.png").is_file(),
            "service_snapshot_present": (run / "returned-service.png").is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("mame", "battery", "settings"):
        parser.add_argument(f"--{name}", type=Path, required=True)
    program = parser.add_mutually_exclusive_group(required=True)
    program.add_argument("--firmware", type=Path, help="mapped MTR 2.x program image")
    program.add_argument("--package", type=Path, help="extracted standard MTR 1.9 package")
    parser.add_argument("--display", choices=("2line", "11line"), required=True)
    parser.add_argument("--startup-frames", type=int, default=1200)
    parser.add_argument("--item", help="Craft menu number; defaults to 728 for MTR 2.x or 227 for the earlier OEM package")
    parser.add_argument("--runtime-contact", action="store_true", help="unfunded contact insertion after pickup without dialing")
    parser.add_argument("--require-contact", action="store_true", help="fail unless the native contact reads match the synthetic fixture")
    parser.add_argument("--output", type=Path, default=ROOT / "build/firmware-acceptance/smartcard-craft")
    args = parser.parse_args()
    if args.startup_frames < 600:
        parser.error("Allow at least 600 startup frames")
    if args.package and args.display != "2line":
        parser.error("The earlier OEM package uses the two-line display")
    args.item = args.item or ("227" if args.package else "728")
    if len(args.item) != 3 or not args.item.isdigit():
        parser.error("Craft menu item must contain three digits")
    out = args.output.resolve() / datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    out.mkdir(parents=True)
    settings = json.loads(args.settings.read_text())
    if not args.runtime_contact and not str(settings.get("pin", "")).isdigit():
        parser.error("A local Craft PIN is required")
    # Do not use a copied terminal identity against a live NCC, including
    # any per-firmware overrides present in the source settings.
    settings["ncc_port"] = int(port())
    for override in settings.get("firmware_overrides", {}).values():
        override["ncc_port"] = settings["ncc_port"]
    local_settings = out / "isolated-settings.json"
    local_settings.write_text(json.dumps(settings), encoding="utf-8")
    env = {key: value for key, value in os.environ.items() if not key.upper().startswith("COINLINE_")}
    if args.package:
        result = run_oem_package(args, out, settings, env)
        (out / "result.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
        print(f"Native Craft contact evidence: {out}\n{json.dumps(result, indent=2)}")
        return 0 if result["native_exit_code"] == 0 and result["result_snapshot_present"] and (
            not args.require_contact or result["contact_data_matches"]) else 1
    command = [sys.executable, str(ROOT / "tools/acceptance/graphic_session.py"),
               "--mame", str(args.mame.resolve()), "--firmware", str(args.firmware.resolve()),
               "--battery", str(args.battery.resolve()), "--settings", str(local_settings),
               "--hardware", "auto", "--display", args.display, "--output", str(out / "terminal"),
               "--quiet", "--card-trace", "--snapshot-interval", "0", "--snapshot-native", "--seconds", "240"]
    native = None
    result = {"native_exit_code": None, "display_review_required": True}
    try:
        with (out / "launcher.log").open("wb") as log:
            native = subprocess.Popen(command, cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT)

            def get_run():
                if native.poll() is not None:
                    raise RuntimeError(f"Native launcher exited {native.returncode}")
                return next(iter((out / "terminal").glob("*/commands.txt")), None)

            commands = wait_for(get_run, 20, "native command file")
            commands.write_text("\n".join(probe_actions(args)) + "\n", encoding="ascii")
            result["run"] = commands.parent.name
            result["native_exit_code"] = native.wait(timeout=240)
            run = commands.parent
            result.update(contact_evidence(run / "mame.log", ROOT / "fixtures/cards/smartcard-sync-test.json"))
            result["result_snapshot_present"] = (run / "card-test-result.png").is_file()
            result["service_snapshot_present"] = (run / "returned-service.png").is_file()
    finally:
        if native is not None and native.poll() is None:
            native.terminate()
            native.wait(timeout=10)
        (out / "result.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(f"Native Craft contact evidence: {out}\n{json.dumps(result, indent=2)}")
    return 0 if result["native_exit_code"] == 0 and result.get("result_snapshot_present") and (
        not args.require_contact or result.get("contact_data_matches")) else 1


if __name__ == "__main__":
    raise SystemExit(main())
