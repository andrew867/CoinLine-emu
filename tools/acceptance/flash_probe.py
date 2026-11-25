#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Erase and program both NOR chips through a synthetic Z180 program and restart."""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def ld_a(value):
    return bytes((0x3e, value))


def out_a(port):
    return bytes((0xed, 0x39, port))


def store_a(address):
    return bytes((0x32, address & 255, address >> 8))


def program_byte(unlock1, unlock2, address, value):
    # D marks whether any erase/program command was needed. Skip when the
    # previously programmed value is already present on the memory bus.
    write = bytes((0x16, 0xa0))
    for location, data in ((unlock1, 0xaa), (unlock2, 0x55), (unlock1, 0x80),
                           (unlock1, 0xaa), (unlock2, 0x55), (address, 0x30)):
        write += ld_a(data) + store_a(location)
    # Poll the selected sector until its erased array value is visible.
    write += bytes((0x3a, address & 255, address >> 8, 0xfe, 0xff, 0x20, 0xf9))
    for location, data in ((unlock1, 0xaa), (unlock2, 0x55),
                           (unlock1, 0xa0), (address, value)):
        write += ld_a(data) + store_a(location)
    write += bytes((0x06, 0xff, 0x10, 0xfe))  # DJNZ delay for byte programming
    compare = bytes((0x3a, address & 255, address >> 8, 0xfe, value))
    return compare + bytes((0x28, len(write))) + write


def synthetic_image():
    # Execute from the common SRAM window while either NOR chip is busy.
    payload = ld_a(0xfe) + out_a(0xc0) + bytes((0x16, 0xa1))
    payload += program_byte(0x555, 0x2aa, 0x1234, 0x5a)
    payload += ld_a(0x80) + out_a(0x39)  # Map the second chip into banked space.
    payload += program_byte(0x1555, 0x12aa, 0x2000, 0xa5)
    payload += bytes((0x7a,)) + store_a(0x9000) + bytes((0x76, 0x18, 0xfd))

    # Bootstrap in flash sector C: set the Z180 common window, copy the
    # diagnostic from flash to RAM, and jump to it.
    boot = bytes((0xf3,)) + ld_a(0xb8) + out_a(0x38) + ld_a(0x81) + out_a(0x3a)
    boot += bytes((0x21, 0x00, 0x02, 0x11, 0x00, 0x80, 0x01, len(payload), 0x00,
                   0xed, 0xb0, 0xc3, 0x00, 0x80))
    image = bytearray(b"\xff" * 1048576)
    image[0xc0000:0xc0000 + len(boot)] = boot
    image[0xc0200:0xc0200 + len(payload)] = payload
    # Programming from zero requires a real sector erase. Place one more
    # byte in each selected sector and one in each neighboring sector.
    for address, value in ((0x1234, 0), (0x1235, 0x33), (0x10010, 0x12),
                           (0x82000, 0), (0x82001, 0x55), (0x90010, 0x34)):
        image[address] = value
    return bytes(image)


PROBE_LUA = """
local machine = manager.machine
local space = machine.devices[":maincpu"].spaces["program"]
local frames = 0
emu.register_frame_done(function()
    frames = frames + 1
    if frames == 180 then
        print(string.format(
            "FLASH_PROBE marker=%02X chip0=%02X chip1=%02X erased=%02X/%02X adjacent=%02X/%02X",
            space:read_u8(0xc1000), space:read_u8(0x1234), space:read_u8(0x82000),
            space:read_u8(0x1235), space:read_u8(0x82001),
            space:read_u8(0x10010), space:read_u8(0x90010)))
        machine:exit()
    end
end, "synthetic flash bus test")
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mame", type=Path, default=ROOT / (
        "third-party/mame/mamecoinline.exe" if os.name == "nt" else "third-party/mame/mamecoinline"))
    parser.add_argument("--rompath", type=Path, default=ROOT / "build/package-inputs/local/roms")
    parser.add_argument("--controller", type=Path, default=ROOT / "firmware/tp_pcd3349a_behavioural.rom")
    parser.add_argument("--output", type=Path, default=ROOT / "build/firmware-acceptance/flash-bus-probe")
    args = parser.parse_args()
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    test = args.output.resolve() / stamp
    test.mkdir(parents=True)
    shutil.copytree(ROOT / "fixtures", test / "fixtures")
    (test / "nvram").mkdir()
    (test / "cfg").mkdir()

    image = synthetic_image()
    digest = hashlib.sha256(image).hexdigest()
    (test / "flash.bin").write_bytes(image)
    entry = {"version_id": "synthetic-flash-bus-test", "friendly_name": "Synthetic flash device test",
             "sha256_hex": digest, "size_bytes": len(image), "motherboard": "later"}
    (test / "fixtures/firmware/firmware-hashes.json").write_text(json.dumps({"entries": [entry]}))
    (test / "probe.lua").write_text(PROBE_LUA)

    env = {key: value for key, value in os.environ.items() if not key.upper().startswith("COINLINE_")}
    env.update(COINLINE_EMU_ROOT=str(test), COINLINE_MOTHERBOARD="later",
               COINLINE_BOARD=str(ROOT / "fixtures/board/board-profile-2line-vfd.json"),
               COINLINE_FIRMWARE=str(test / "flash.bin"), COINLINE_CARD_DMA_CHANNEL="1",
               COINLINE_DIAGNOSTICS="0", COINLINE_TP8048_ROM=str(args.controller.resolve()))
    command = [str(args.mame.resolve()), "cl_millennium", "-noreadconfig", "-skip_gameinfo",
               "-nodebug", "-nolog", "-video", "none", "-sound", "none",
               "-rompath", str(args.rompath.resolve()), "-nvram_directory", str(test / "nvram"),
               "-cfg_directory", str(test / "cfg"), "-seconds_to_run", "4",
               "-autoboot_delay", "0", "-autoboot_script", str(test / "probe.lua")]
    print(test, flush=True)
    for phase, marker, load_message in (("cold", "A0", "program flash seed"),
                                       ("warm", "A1", "program flash load")):
        result = subprocess.run(command, cwd=test, env=env, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, timeout=60)
        log = result.stdout.decode(errors="replace")
        (test / f"{phase}.log").write_text(log, encoding="utf-8")
        expected = f"FLASH_PROBE marker={marker} chip0=5A chip1=A5 erased=FF/FF adjacent=12/34"
        if result.returncode or expected not in log or load_message not in log:
            raise RuntimeError(f"{phase} flash probe failed")
        raw = (test / "nvram/cl_millennium/program_flash").read_bytes()
        if (len(raw) != 64 + len(image) or raw[:64].decode() != digest
                or raw[64 + 0x1234:64 + 0x1236] != b"\x5a\xff"
                or raw[64 + 0x82000:64 + 0x82002] != b"\xa5\xff"
                or raw[64 + 0x10010] != 0x12 or raw[64 + 0x90010] != 0x34
                or (test / "flash.bin").read_bytes() != image):
            raise RuntimeError(f"{phase} persistent flash verification failed")
        print(f"{phase} flash bus verified", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
