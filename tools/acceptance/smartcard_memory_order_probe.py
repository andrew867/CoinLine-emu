#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Qualify non-payment memory addressing through both native contact buses."""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]

PROBE = r'''
local machine = manager.machine
local io = machine.devices[":maincpu"].spaces["io"]
local card = assert(machine.ioport.ports[":CARDUI"]:field(2))
local earlier = os.getenv("COINLINE_MOTHERBOARD") == "earlier"
local programming = os.getenv("COINLINE_SYNC_PROGRAM_BENCH") == "1"
local warm = os.getenv("COINLINE_SYNC_PROGRAM_WARM") == "1"
local expected = {}
for byte in (os.getenv("COINLINE_SYNC_EXPECTED_HEX") or "963CA700"):gmatch("%x%x") do
    expected[#expected + 1] = assert(tonumber(byte, 16))
end
local frame = 0
local function pins(reset, clock)
    if earlier then
        io:write_u8(0x42, (reset and 0 or 0x80) | (clock and 0 or 0x40))
    else
        io:write_u8(0xc1, 4 | (clock and 8 or 0))
        io:write_u8(0xc2, 8 | (reset and 4 or 0))
    end
end
local function power()
    if earlier then
        io:write_u8(0x42, 0xc0)
        io:write_u8(0x40, 0); io:write_u8(0x40, 0x40); io:write_u8(0x40, 0)
    else
        io:write_u8(0xc1, 4); io:write_u8(0xc2, 0); io:write_u8(0xc3, 0)
        io:write_u8(0xc2, 8); io:write_u8(0xc3, 2); io:write_u8(0xc3, 0)
    end
end
local function seek_zero()
    pins(true, false); pins(true, true); pins(true, false); pins(false, false)
end
local function advance() pins(false, true); pins(false, false) end
local function data_high()
    return (io:read_u8(earlier and 0x42 or 0xc0) & (earlier and 4 or 2)) ~= 0
end
local function seek_bit(address)
    seek_zero()
    for i = 1, address do advance() end
end
local function start_program()
    pins(true, false); pins(false, false); pins(false, true)
end
local function read_bits(count)
    local value = 0
    for i = 1, count do
        local high = data_high()
        value = (value << 1) | (high and 1 or 0)
        advance()
    end
    return value
end
local function present() return (io:read_u8(earlier and 0x80 or 0x40) & 6) == 6 end
local function read_memory()
    for index, byte in ipairs(expected) do
        assert(read_bits(8) == byte, "incorrect memory bit/byte order at index " .. index)
    end
end
emu.register_frame_done(function()
    frame = frame + 1
    if frame == 5 then card:set_value(1) end
    if frame == 11 then card:clear_value() end
    if frame == 16 then
        assert(present(), "insertion not observed")
        power(); seek_zero()
        if warm then expected[#expected] = expected[#expected] & 0x7f end
        read_memory()
        assert(read_bits(8) == 0xff, "out-of-range contact did not release")
        seek_zero()
        for i = 1, 8 do advance() end
        assert(read_bits(16) == (expected[2] << 8 | expected[3]), "byte address seek incorrect")
        seek_zero()
        assert(read_bits(8) == expected[1], "reset did not seek to byte zero")
        if warm then
            print("SMARTCARD_SYNC_BENCH_PERSISTENCE passed")
            machine:exit(); return
        end
        if programming then
            -- The last byte of this invented pattern contains the bounded
            -- programmable bench bits. It is not a historical card layout.
            seek_bit((#expected - 1) * 8)
            assert(data_high(), "bench bit initially low")
            start_program(); pins(false, false)
            assert(data_high(), "sub-minimum pulse modified the test bit")
            start_program()
        else
            card:set_value(1)
        end
    end
    if programming then
        if frame == 19 then
            pins(false, false)
            assert(not data_high(), "completed pulse did not program the test bit")
            expected[#expected] = expected[#expected] & 0x7f
            seek_bit(0)
            assert(data_high(), "read-only control bit unexpectedly low")
            start_program()
        end
        if frame == 22 then
            pins(false, false)
            assert(data_high(), "programming crossed the permitted bench range")
            seek_bit((#expected - 1) * 8 + 1)
            assert(data_high(), "neighbouring bench bit changed")
            start_program()
            card:set_value(1)
        end
        if frame == 28 then card:clear_value() end
        if frame == 36 then
            assert(not present(), "removal during open programming pulse not observed")
            pins(false, false)
            card:set_value(1)
        end
        if frame == 42 then card:clear_value() end
        if frame == 52 then
            assert(present(), "reinsertion not observed")
            power(); seek_zero()
            read_memory()
            assert(read_bits(8) == 0xff, "out-of-range contact did not release")
            print("SMARTCARD_SYNC_BENCH passed")
            machine:exit()
        end
        return
    end
    if frame == 22 then card:clear_value() end
    if frame == 32 then
        assert(not present(), "removal not observed")
        -- With the contact deselected, the port may return its output latch.
        -- The physical presence sensors establish removal, not that latch.
        card:set_value(1)
    end
    if frame == 38 then card:clear_value() end
    if frame == 48 then
        assert(present(), "reinsertion not observed")
        power(); seek_zero()
        read_memory()
        print("SMARTCARD_MEMORY_ORDER passed")
        machine:exit()
    end
end, "non-payment contact addressing")
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mame", required=True, type=Path)
    parser.add_argument("--rompath", type=Path, default=ROOT / "build/package-inputs/local/roms")
    parser.add_argument("--controller", type=Path, default=ROOT / "firmware/tp_pcd3349a_behavioural.rom")
    parser.add_argument("--output", type=Path, default=ROOT / "build/firmware-acceptance/smartcard-memory-order")
    parser.add_argument("--lab-profile", choices=("50", "500", "empty"),
                        help="read an entire packaged nonredeemable synchronous LAB snapshot")
    parser.add_argument("--gpm103-contact", action="store_true",
                        help="qualify the bounded, non-payment EEPROM contact fixture and persistent writes")
    parser.add_argument("--shadybucks-contact", action="store_true",
                        help="generate a new independent SBX1 read-only snapshot and qualify contact reads")
    args = parser.parse_args()
    if sum((bool(args.lab_profile), args.gpm103_contact, args.shadybucks_contact)) > 1:
        parser.error("choose one fixture")
    root = args.output.resolve() / datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    fixture = ROOT / "fixtures/cards" / (
        "smartcard-sync-gpm103-contact.json" if args.gpm103_contact else
        f"smartcard-sync-lab-purse-{args.lab_profile}.json" if args.lab_profile else "smartcard-sync-address-test.json")
    if args.shadybucks_contact:
        sys.path.insert(0, str(ROOT / "tools"))
        from shadybucks import ShadyBucks
        root.mkdir(parents=True)
        issuer = ShadyBucks(root / "local-demo.sqlite")
        card = issuer.issue(50)["fixture_id"]
        fixture = root / "independent-sbx1.json"
        fixture.write_text(json.dumps(issuer.export_contact(card), indent=2) + "\n", encoding="utf-8")
        initial_state = issuer.balance(card)
    expected = json.loads(fixture.read_text(encoding="utf-8"))["memory_hex"]
    for board in ("earlier", "later"):
        out = root / board
        out.mkdir(parents=True)
        shutil.copytree(ROOT / "fixtures", out / "fixtures")
        image = bytearray(b"\xff" * 1048576)
        entry_offset = 0 if board == "earlier" else 0xc0000
        image[entry_offset:entry_offset + 4] = b"\xf3\x76\x18\xfd"
        seed = out / "flash.bin"
        seed.write_bytes(image)
        entry = {"version_id": "synthetic-memory-order", "friendly_name": "Synthetic bounded contact test",
                 "sha256_hex": hashlib.sha256(image).hexdigest(), "size_bytes": len(image), "motherboard": board}
        (out / "fixtures/firmware/firmware-hashes.json").write_text(json.dumps({"entries": [entry]}))
        lua = out / "probe.lua"
        lua.write_text(PROBE, encoding="ascii")
        env = {k: v for k, v in os.environ.items() if not k.upper().startswith("COINLINE_")}
        env.update(COINLINE_EMU_ROOT=str(out), COINLINE_MOTHERBOARD=board, COINLINE_FIRMWARE=str(seed),
                   COINLINE_BOARD=str(ROOT / "fixtures/board/board-profile-2line-vfd.json"), COINLINE_DIAGNOSTICS="0",
                   COINLINE_HARDWARE_PRESET="multipay-v1" if board == "earlier" else "multipay-v1.3",
                   COINLINE_TP8048_ROM=str(args.controller.resolve()),
                   COINLINE_SMART_CARD=str(fixture), COINLINE_SYNC_EXPECTED_HEX=expected)
        if args.gpm103_contact:
            env.update(COINLINE_SYNC_PROGRAM_BENCH="1", COINLINE_SMART_CARD_TRACE="1")
        command = [str(args.mame.resolve()), "cl_millennium", "-noreadconfig", "-skip_gameinfo", "-nodebug", "-nolog",
                   "-video", "none", "-sound", "none", "-rompath", str(args.rompath.resolve()),
                   "-nvram_directory", str(out / "nvram"), "-cfg_directory", str(out / "cfg"),
                   "-seconds_to_run", "4", "-autoboot_delay", "0", "-autoboot_script", str(lua)]
        result = subprocess.run(command, cwd=out, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=90)
        log = result.stdout.decode(errors="replace")
        (out / "mame.log").write_text(log, encoding="utf-8")
        marker = "SMARTCARD_SYNC_BENCH passed" if args.gpm103_contact else "SMARTCARD_MEMORY_ORDER passed"
        if result.returncode or "LUA ERROR" in log or marker not in log:
            raise RuntimeError(f"Native {board} memory-order test failed: {out}")
        print(f"Native {board} memory-order test passed: {out}", flush=True)
        if args.gpm103_contact:
            warm = subprocess.run(command, cwd=out, env=dict(env, COINLINE_SYNC_PROGRAM_WARM="1"),
                                  stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=90)
            warm_log = warm.stdout.decode(errors="replace")
            (out / "warm.log").write_text(warm_log, encoding="utf-8")
            if (warm.returncode or "LUA ERROR" in warm_log or
                    "SMARTCARD_SYNC_BENCH_PERSISTENCE passed" not in warm_log or
                    "smart-card synthetic memory restored" not in warm_log):
                raise RuntimeError(f"Native {board} restart persistence test failed: {out}")
            print(f"Native {board} restart persistence test passed: {out}", flush=True)
    if args.shadybucks_contact and issuer.balance(card) != initial_state:
        raise RuntimeError("Read-only contact probe changed the independent local ledger")


if __name__ == "__main__":
    main()
