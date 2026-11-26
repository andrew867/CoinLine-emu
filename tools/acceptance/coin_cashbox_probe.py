#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Native later-board validator UART and cashbox inputs, isolated lab coins."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[2]

PROBE = r'''
local machine = manager.machine
local io = machine.devices[":maincpu"].spaces["io"]
local coin = assert(machine.ioport.ports[":COINUI"])
local box = assert(machine.ioport.ports[":CASHBOXUI"])
local vault = assert(machine.ioport.ports[":SECMASK"]:field(4))
local earlier = os.getenv("COINLINE_MOTHERBOARD") == "earlier"
local warm = os.getenv("COINLINE_COIN_WARM_PROBE") == "1"
local removed, full = assert(box:field(1)), assert(box:field(2))
local frame = 0
local function tx(value)
    assert(io:read_u8(0xe5) & 0x20 ~= 0, "UART transmitter not ready")
    io:write_u8(0xe0, string.byte(value))
end
local function rx(...)
    for _, byte in ipairs({...}) do
        local lsr = io:read_u8(0xe5)
        assert(lsr & 1 ~= 0 and lsr & 0x1e == 0, "UART receive status failure")
        local got = io:read_u8(0xe0)
        assert(got == byte, string.format("UART expected %02X got %02X", byte, got))
    end
end
local function pulse(field) field:set_value(1) end
local function raw_status()
    return io:read_u8(earlier and 0x80 or 0xc0) & (earlier and 0xc0 or 0x0c)
end
emu.register_frame_done(function()
    frame = frame + 1
    if warm then
        if frame == 12 then print("COIN_CASHBOX_WARM passed"); machine:exit() end
        return
    end
    if earlier then
        if frame == 4 then assert(raw_status() == 0xc0); pulse(vault) end
        if frame == 8 then vault:clear_value(); pulse(removed) end
        if frame == 12 then removed:clear_value() end
        if frame >= 4 and frame <= 22 then raw_status() end
        if frame == 22 then assert(raw_status() == 0); pulse(removed); pulse(vault) end
        if frame == 26 then removed:clear_value(); vault:clear_value() end
        if frame >= 22 and frame <= 40 then raw_status() end
        if frame == 40 then assert(raw_status() == 0xc0); print("COIN_CASHBOX_EARLIER passed"); machine:exit() end
        return
    end
    if frame == 4 then
        assert(raw_status() == 0)
        -- 6.144 MHz UART clock / (16 * 640) = 600 baud, 8N1, RX FIFO.
        io:write_u8(0xe3, 0x80); io:write_u8(0xe0, 0x80); io:write_u8(0xe1, 2)
        io:write_u8(0xe3, 3); io:write_u8(0xe2, 1)
    end
    if frame == 6 then tx('g') end
    if frame == 12 then rx(0x41); tx('a') end
    if frame == 16 then pulse(coin:field(8)) end
    if frame == 22 then coin:field(8):clear_value(); rx(0x31) end
    if frame == 25 then pulse(coin:field(16)) end
    if frame == 31 then coin:field(16):clear_value(); rx(0x32); tx('c') end
    if frame == 36 then tx('r') end
    if frame == 44 then rx(0x42, 0, 8) end
    if frame == 160 then tx('r') end
    if frame == 168 then rx(0x42, 0, 0); pulse(full) end
    if frame == 172 then full:clear_value(); tx('r') end
    if frame == 180 then rx(0x42, 0, 0x40); pulse(coin:field(32)) end
    if frame == 186 then coin:field(32):clear_value(); rx(0x33); tx('c') end
    if frame == 315 then tx('r') end
    if frame == 323 then rx(0x42, 2, 0x40); pulse(vault); pulse(removed) end
    if frame == 327 then vault:clear_value(); removed:clear_value() end
    if frame >= 323 and frame <= 340 then raw_status() end
    if frame == 340 then assert(raw_status() == 0x0c); pulse(removed); pulse(full); pulse(vault) end
    if frame == 344 then removed:clear_value(); full:clear_value(); vault:clear_value() end
    if frame >= 340 and frame <= 355 then raw_status() end
    if frame == 355 then assert(raw_status() == 0); tx('f') end
    if frame == 485 then tx('r') end
    if frame == 493 then rx(0x42,0,0); pulse(coin:field(4)) end
    if frame == 498 then coin:field(4):clear_value(); tx('g') end
    if frame == 505 then rx(0x49); tx('l') end
    if frame == 510 then tx('g') end
    if frame == 518 then rx(0x41); print("COIN_CASHBOX_LATER passed"); machine:exit() end
end)
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mame", type=Path, required=True)
    parser.add_argument("--rompath", type=Path, default=ROOT / "build/package-inputs/local/roms")
    parser.add_argument("--controller", type=Path, default=ROOT / "firmware/tp_pcd3349a_behavioural.rom")
    parser.add_argument("--output", type=Path, default=ROOT / "build/firmware-acceptance/coin-cashbox-probe")
    args = parser.parse_args()
    root = args.output.resolve() / datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    for board in ("earlier", "later"):
        out = root / board; out.mkdir(parents=True)
        shutil.copytree(ROOT / "fixtures", out / "fixtures")
        image = bytearray(b"\xff" * 1048576)
        offset = 0 if board == "earlier" else 0xc0000
        image[offset:offset+4] = b"\xf3\x76\x18\xfd"
        seed = out / "flash.bin"; seed.write_bytes(image)
        entry = {"version_id": "synthetic-coin-cashbox", "friendly_name": "Synthetic cashbox and UART probe",
                 "sha256_hex": hashlib.sha256(image).hexdigest(), "size_bytes": len(image), "motherboard": board}
        (out / "fixtures/firmware/firmware-hashes.json").write_text(json.dumps({"entries": [entry]}))
        lua = out / "probe.lua"; lua.write_text(PROBE, encoding="ascii")
        env = {k: v for k, v in os.environ.items() if not k.upper().startswith("COINLINE_")}
        env.update(COINLINE_EMU_ROOT=str(out), COINLINE_MOTHERBOARD=board, COINLINE_FIRMWARE=str(seed),
                   COINLINE_HARDWARE_PRESET="multipay-v1" if board == "earlier" else "multipay-v1.3",
                   COINLINE_BOARD=str(ROOT / "fixtures/board/board-profile-2line-vfd.json"), COINLINE_DIAGNOSTICS="0",
                   COINLINE_TP8048_ROM=str(args.controller.resolve()))
        command = [str(args.mame.resolve()), "cl_millennium", "-noreadconfig", "-skip_gameinfo", "-nodebug", "-nolog",
                   "-video", "none", "-sound", "none", "-rompath", str(args.rompath.resolve()),
                   "-nvram_directory", str(out / "nvram"), "-cfg_directory", str(out / "cfg"),
                   "-seconds_to_run", "12", "-autoboot_delay", "0", "-autoboot_script", str(lua)]
        result = subprocess.run(command, cwd=out, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=90)
        log = result.stdout.decode(errors="replace"); (out / "mame.log").write_text(log, encoding="utf-8")
        if result.returncode or f"COIN_CASHBOX_{board.upper()} passed" not in log:
            raise RuntimeError(f"Native {board} cashbox probe failed: {out}")
        if board == "later":
            file = out / "nvram/cl_millennium/coin"
            raw = file.read_bytes()
            if raw != b"CLCOIN1\0" + struct.pack("<QQQQ", 0, 0, 25, 15):
                raise RuntimeError(f"Unexpected physical coin contents: {out}")
            warm = subprocess.run(command, cwd=out, env=dict(env, COINLINE_COIN_WARM_PROBE="1"), stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, timeout=90)
            warm_log = warm.stdout.decode(errors="replace"); (out / "warm.log").write_text(warm_log, encoding="utf-8")
            if warm.returncode or "COIN_CASHBOX_WARM passed" not in warm_log or file.read_bytes() != raw:
                raise RuntimeError(f"Physical coin restart verification failed: {out}")
        print(f"Native {board} cashbox probe passed: {out}", flush=True)


if __name__ == "__main__":
    main()
