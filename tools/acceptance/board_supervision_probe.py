#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Exercise the provisional CPU-only watchdog and independent NMI/rail inputs."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]

PROBE = r'''
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local power = assert(machine.ioport.ports[":POWERUI"])
local nmi, rail = assert(power:field(1)), assert(power:field(2))
local frame = 0
local function marker() return (cpu.state["DE"].value >> 8) & 255 end
emu.register_frame_done(function()
    frame = frame + 1
    if frame == 8 then assert(marker() == 0x55, "synthetic CPU did not boot"); nmi:set_value(1) end
    if frame == 18 then assert(marker() == 0xa6, "NMI handler did not run"); nmi:clear_value() end
    if frame == 80 then assert(marker() == 0x55, "watchdog did not reset CPU") end
    if frame == 90 then rail:set_value(1) end
    if frame == 95 then nmi:set_value(1) end
    if frame == 160 then assert(marker() == 0x55, "NMI ran while control rail unavailable"); nmi:clear_value() end
    if frame == 175 then rail:clear_value() end
    if frame == 185 then assert(marker() == 0x55, "CPU did not restart after rail restoration")
        print("BOARD_SUPERVISION_PROBE passed"); machine:exit() end
end)
'''


def synthetic_image():
    # Common SRAM stack, a positive watchdog pulse and an interruptible HALT.
    main = bytes((0xf3, 0x3e, 0xb8, 0xed, 0x39, 0x38, 0x3e, 0x81, 0xed, 0x39, 0x3a,
                  0x31, 0x00, 0x90, 0x16, 0x55,
                  0x3e, 0x00, 0xed, 0x39, 0x63,
                  0x3e, 0x80, 0xed, 0x39, 0x63, 0x00, 0x00,
                  0x3e, 0x00, 0xed, 0x39, 0x63, 0x76, 0x18, 0xfd))
    nmi = bytes((0x16, 0xa6, 0xed, 0x45))  # LD D,A6; RETN
    image = bytearray(b"\xff" * 1048576)
    image[0xc0000:0xc0000 + len(main)] = main
    image[0xc0066:0xc0066 + len(nmi)] = nmi
    return image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mame", type=Path, required=True)
    parser.add_argument("--rompath", type=Path, default=ROOT / "build/package-inputs/local/roms")
    parser.add_argument("--controller", type=Path, default=ROOT / "firmware/tp_pcd3349a_behavioural.rom")
    parser.add_argument("--output", type=Path, default=ROOT / "build/firmware-acceptance/board-supervision-probe")
    args = parser.parse_args()
    out = args.output.resolve() / datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    out.mkdir(parents=True)
    shutil.copytree(ROOT / "fixtures", out / "fixtures")
    image = synthetic_image()
    seed = out / "flash.bin"
    seed.write_bytes(image)
    entry = {"version_id": "synthetic-board-supervision", "friendly_name": "Synthetic supervision probe",
             "sha256_hex": hashlib.sha256(image).hexdigest(), "size_bytes": len(image), "motherboard": "later"}
    (out / "fixtures/firmware/firmware-hashes.json").write_text(json.dumps({"entries": [entry]}))
    lua = out / "probe.lua"
    lua.write_text(PROBE, encoding="ascii")
    env = {key: value for key, value in os.environ.items() if not key.upper().startswith("COINLINE_")}
    env.update(COINLINE_EMU_ROOT=str(out), COINLINE_MOTHERBOARD="later", COINLINE_HARDWARE_PRESET="multipay-v1.3",
               COINLINE_BOARD=str(ROOT / "fixtures/board/board-profile-2line-vfd.json"), COINLINE_FIRMWARE=str(seed),
               COINLINE_DIAGNOSTICS="0", COINLINE_TP8048_ROM=str(args.controller.resolve()),
               COINLINE_BOARD_SUPERVISION_PROFILE="cp-only", COINLINE_BOARD_SUPERVISION_TRACE="1")
    command = [str(args.mame.resolve()), "cl_millennium", "-noreadconfig", "-skip_gameinfo", "-nodebug", "-nolog",
               "-video", "none", "-sound", "none", "-rompath", str(args.rompath.resolve()),
               "-nvram_directory", str(out / "nvram"), "-cfg_directory", str(out / "cfg"),
               "-seconds_to_run", "5", "-autoboot_delay", "0", "-autoboot_script", str(lua)]
    result = subprocess.run(command, cwd=out, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=90)
    log = result.stdout.decode(errors="replace")
    (out / "mame.log").write_text(log, encoding="utf-8")
    expected = ["BOARD_SUPERVISION_PROBE passed", "board control NMI asserted", "board watchdog deadline expired",
                "board control rail unavailable", "board control rail restored"]
    if result.returncode or any(message not in log for message in expected) or log.count("board control NMI asserted") != 1:
        raise RuntimeError(f"Native supervision probe failed: {out}")
    if log.count("board watchdog pulse") < 3 or log.count("board watchdog deadline expired") != 1:
        raise RuntimeError(f"Unexpected watchdog service/reset count: {out}")
    print(f"Native provisional CPU supervision passed: {out}")


if __name__ == "__main__":
    main()
