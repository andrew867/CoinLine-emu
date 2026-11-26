#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Native bus test with a synthetic contact-memory pattern, not a payment card."""
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
local io = machine.devices[":maincpu"].spaces["io"]
local card = assert(machine.ioport.ports[":CARDUI"]:field(2))
local frame = 0
local warm = os.getenv("COINLINE_SMART_CARD_WARM_PROBE") == "1"
local function d(v) io:write_u8(0xc1, v) end
local function e(v) io:write_u8(0xc2, v) end
local function f(v) io:write_u8(0xc3, v) end
local function bit() return (io:read_u8(0xc0) & 2) ~= 0 end
local base = 7
local function select_card(address)
    base = 4 | (address or 3)
    d(base); e(0); f(0); e(8); f(2); f(0)
    e(12); d(base | 8); d(base); e(8)
end
local function advance() d(base | 8); d(base) end
local function program() e(12); e(8); d(base | 8) end
emu.register_frame_done(function()
    frame = frame + 1
    if frame == 5 then card:set_value(1) end
    if frame == 11 then card:clear_value() end
    if frame == 12 then
        assert(io:read_u8(0x40) & 6 == 6, "card not inserted")
        select_card()
        assert(bit(), "initial contact bit missing")
        advance()
        if warm then
            assert(not bit(), "terminal restart restored programmed contact bit")
            print("SMARTCARD_PIN_PERSISTENCE passed")
            e(0); machine:exit(); return
        end
        assert(bit(), "initial writable bit missing")
        program(); d(base) -- too short
        assert(bit(), "short pulse changed memory")
        program() -- hold through the next frame
    end
    if frame == 14 then
        d(base)
        assert(not bit(), "timed programming did not clear the bit")
        select_card()
        assert(bit(), "read-only neighbour modified")
        advance()
        assert(not bit(), "reset restored the programmed bit")
        e(0)
        card:set_value(1)
    end
    if frame == 20 then card:clear_value() end
    if frame == 32 then
        assert(io:read_u8(0x40) & 6 == 0, "card not removed")
        card:set_value(1)
    end
    if frame == 38 then card:clear_value() end
    if frame == 45 then
        assert(io:read_u8(0x40) & 6 == 6, "card not reinserted")
        select_card(0); assert(bit(), "read-only neighbour lost")
        advance(); assert(not bit(), "reinsertion restored programmed bit")
        print("SMARTCARD_PIN_PROBE passed")
        e(0); machine:exit()
    end
end, "synthetic smart-card contacts")
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mame", type=Path, required=True)
    parser.add_argument("--rompath", type=Path, default=ROOT / "build/package-inputs/local/roms")
    parser.add_argument("--controller", type=Path, default=ROOT / "firmware/tp_pcd3349a_behavioural.rom")
    parser.add_argument("--output", type=Path, default=ROOT / "build/firmware-acceptance/smartcard-pin-probe")
    parser.add_argument("--program-us", type=int, choices=(1000, 10000), default=1000,
                        help="synthetic test-region programming delay, not a historical card specification")
    args = parser.parse_args()
    out = args.output.resolve() / datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    out.mkdir(parents=True)
    shutil.copytree(ROOT / "fixtures", out / "fixtures")
    image = bytearray(b"\xff" * 1048576)
    image[0xc0000:0xc0004] = b"\xf3\x76\x18\xfd"  # DI; HALT; JR back
    seed = out / "flash.bin"
    seed.write_bytes(image)
    entry = {"version_id": "synthetic-card-pin-probe", "friendly_name": "Synthetic contact probe",
             "sha256_hex": hashlib.sha256(image).hexdigest(), "size_bytes": len(image), "motherboard": "later"}
    (out / "fixtures/firmware/firmware-hashes.json").write_text(json.dumps({"entries": [entry]}))
    fixture = out / "synthetic-memory.json"
    fixture.write_text(json.dumps({"protocol": "sync", "atr_hex": "00", "memory_hex": "FFFF",
        "sync_program_enabled": True, "sync_program_first_bit": 1, "sync_program_end_bit": 2,
        "sync_program_us": args.program_us}))
    lua = out / "probe.lua"
    lua.write_text(PROBE)
    env = {key: value for key, value in os.environ.items() if not key.upper().startswith("COINLINE_")}
    env.update(COINLINE_EMU_ROOT=str(out), COINLINE_MOTHERBOARD="later",
        COINLINE_BOARD=str(ROOT / "fixtures/board/board-profile-2line-vfd.json"), COINLINE_FIRMWARE=str(seed),
        COINLINE_DIAGNOSTICS="0", COINLINE_TP8048_ROM=str(args.controller.resolve()),
        COINLINE_SMART_CARD=str(fixture), COINLINE_SMART_CARD_TRACE="1")
    command = [str(args.mame.resolve()), "cl_millennium", "-noreadconfig", "-skip_gameinfo", "-nodebug",
        "-nolog", "-video", "none", "-sound", "none", "-rompath", str(args.rompath.resolve()),
        "-nvram_directory", str(out / "nvram"), "-cfg_directory", str(out / "cfg"),
        "-seconds_to_run", "4", "-autoboot_delay", "0", "-autoboot_script", str(lua)]
    result = subprocess.run(command, cwd=out, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=90)
    log = result.stdout.decode(errors="replace")
    (out / "mame.log").write_text(log, encoding="utf-8")
    if result.returncode or "SMARTCARD_PIN_PROBE passed" not in log:
        raise RuntimeError(f"Native contact-memory probe failed: {out}")
    warm = subprocess.run(command, cwd=out, env=dict(env, COINLINE_SMART_CARD_WARM_PROBE="1"),
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=90)
    warm_log = warm.stdout.decode(errors="replace")
    (out / "warm.log").write_text(warm_log, encoding="utf-8")
    if warm.returncode or "SMARTCARD_PIN_PERSISTENCE passed" not in warm_log or "smart-card synthetic memory restored" not in warm_log:
        raise RuntimeError(f"Native restart contact-memory probe failed: {out}")
    print(f"Native contact-memory and restart persistence probes passed: {out}")


if __name__ == "__main__":
    main()
