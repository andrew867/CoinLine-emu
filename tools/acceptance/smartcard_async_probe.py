#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Exercise synthetic, non-payment T=0/T=1 through native Z180 ASCI0 pins."""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def block(pcb, payload):
    frame = bytes((0, pcb, len(payload))) + bytes(payload)
    check = 0
    for byte in frame:
        check ^= byte
    return frame + bytes((check,))


def synthetic_program(protocol):
    code = bytearray((0xf3,))  # DI
    errors = []
    inverse = protocol == "t0-inverse"
    receive_index = 0

    def wire(value):
        if not inverse:
            return value
        return int(f"{value ^ 255:08b}"[::-1], 2)

    def emit(*values):
        code.extend(values)

    def out(port, value):
        emit(0x3e, value, 0xed, 0x39, port)  # LD A,n; OUT0 (port),A

    def delay(count):
        # Keep the terminal guard interval and allow card activation to
        # run at the emulated clock; no intervention in ASCI registers.
        emit(0x01, count & 255, count >> 8, 0x0b, 0x78, 0xb1, 0x20, 0xfb)

    def poll(port, mask, expected):
        emit(0xed, 0x38, port, 0xe6, mask, 0xfe, expected, 0x20, 0xf7)

    def assert_a(expected):
        emit(0xfe, expected, 0xc2, 0, 0)  # CP n; JP NZ, failure
        errors.append(len(code) - 2)

    def rx(values):
        nonlocal receive_index
        for value in values:
            receive_index += 1
            emit(0x1e, receive_index & 255)  # failing receive index in E
            poll(0x04, 0x80, 0x80)  # RDRF
            emit(0xed, 0x38, 0x08, 0x47)  # RDR0; LD B,A (latched byte)
            emit(0xed, 0x38, 0x04, 0xe6, 0x70)
            assert_a(0)  # no overrun, parity or framing error
            emit(0x78)  # LD A,B
            assert_a(wire(value))

    def tx(values):
        for value in values:
            poll(0x04, 0x02, 0x02)  # TDRE
            out(0x06, wire(value))  # TDR0
            delay(0x0400)

    emit(0x16, 0x10)  # stage marker D
    poll(0x40, 6, 6)  # wait for both card insertion switches
    out(0xc0, 0xf0)  # clear only async gate bit in the reset latch
    out(0xc1, 7)     # customer contact, manual output released
    out(0xc2, 0)
    out(0xc3, 0)
    out(0xc2, 8)
    out(0xc3, 2)
    out(0xc3, 0)
    delay(0x4000)
    out(0x02, 0x12 if inverse else 2)  # ASCI0 6.144 MHz / 640 = 9600 baud; inverse uses odd parity
    out(0x00, 0x66)  # receive, transmit, eight data bits, parity, one stop
    out(0xc2, 9)     # generated card clock, reset held low
    delay(0x4000)
    emit(0x16, 0x20)
    out(0xc2, 13)    # release external card reset
    rx((0x3b, 0x10, 0x12) if protocol == "t0-pps" else
       (0x3f if inverse else 0x3b, 0) if protocol != "t1" else (0x3b, 0x80, 1, 0x81))
    if protocol == "t0-pps":
        pps = (0xff, 0x10, 0x12, 0xfd)
        tx(pps); rx(pps)
        out(0x02, 1)  # change terminal to 19200 only after the complete echo
        delay(0x0400)
    emit(0x16, 0x30)
    select = (0x00, 0xa4, 4, 0, 5, 0xf0, 0x4c, 0x41, 0x42, 1)
    read = (0, 0xb0, 0, 0, 4)
    if protocol != "t1":
        tx(select[:5]); rx((0xa4,)); tx(select[5:]); rx((0x90, 0))
        emit(0x16, 0x40)
        tx(read); rx((0xb0, ord("T"), ord("E"), ord("S"), ord("T"), 0x90, 0))
    else:
        tx(block(0, select)); rx(block(0, (0x90, 0)))
        tx(block(0xc1, (3,))); rx(block(0xe1, (3,)))
        emit(0x16, 0x40)
        tx(block(0x40, read)); rx(block(0x60, (ord("T"), ord("E"), ord("S"))))
        tx(block(0x80, ())); rx(block(0, (ord("T"), 0x90, 0)))
    out(0xc2, 0)    # power off and disconnect the contact
    emit(0x16, 0xa5, 0x76, 0x18, 0xfd)  # success; HALT loop
    fail = len(code)
    emit(0x76, 0x18, 0xfd)  # retain failing stage and CPU state
    for offset in errors:
        code[offset:offset + 2] = fail.to_bytes(2, "little")
    return bytes(code)


LUA = r'''
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local card = assert(machine.ioport.ports[":CARDUI"]:field(2))
local frame = 0
emu.register_frame_done(function()
    frame = frame + 1
    if frame == 5 then card:set_value(1) end
    if frame == 11 then card:clear_value() end
    local marker = (cpu.state["DE"].value >> 8) & 255
    if marker == 0xa5 or frame == 240 then
        print(string.format("SMARTCARD_ASYNC_PROBE marker=%02X PC=%04X AF=%04X DE=%04X", marker,
            cpu.state["PC"].value, cpu.state["AF"].value, cpu.state["DE"].value))
        machine:exit()
    end
end, "synthetic native smart-card UART")
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mame", required=True, type=Path)
    parser.add_argument("--protocol", choices=("t0", "t1", "t0-inverse", "t0-pps", "all"), default="all")
    parser.add_argument("--pps-spacing-us", type=int, default=1500, help="declared synthetic PPS card inter-character delay")
    parser.add_argument("--rompath", type=Path, default=ROOT / "build/package-inputs/local/roms")
    parser.add_argument("--controller", type=Path, default=ROOT / "firmware/tp_pcd3349a_behavioural.rom")
    parser.add_argument("--output", type=Path, default=ROOT / "build/firmware-acceptance/smartcard-async-probe")
    args = parser.parse_args()
    root = args.output.resolve() / datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    root.mkdir(parents=True)
    for protocol in ("t0", "t1", "t0-inverse", "t0-pps") if args.protocol == "all" else (args.protocol,):
        out = root / protocol
        out.mkdir()
        shutil.copytree(ROOT / "fixtures", out / "fixtures")
        image = bytearray(b"\xff" * 1048576)
        program = synthetic_program(protocol)
        image[0xc0000:0xc0000 + len(program)] = program
        seed = out / "flash.bin"
        seed.write_bytes(image)
        entry = {"version_id": "synthetic-smartcard-uart", "friendly_name": "Synthetic contact UART probe",
                 "sha256_hex": hashlib.sha256(image).hexdigest(), "size_bytes": len(image), "motherboard": "later"}
        (out / "fixtures/firmware/firmware-hashes.json").write_text(json.dumps({"entries": [entry]}))
        fixture = ROOT / f"fixtures/cards/smartcard-async-{protocol}-lab.json"
        if protocol in ("t0-inverse", "t0-pps"):
            inverse = json.loads((ROOT / "fixtures/cards/smartcard-async-t0-lab.json").read_text())
            inverse["atr_hex"] = "3F 00" if protocol == "t0-inverse" else "3B 10 12"
            if protocol == "t0-pps":
                inverse["inter_character_delay_us"] = args.pps_spacing_us
            fixture = out / "special-lab.json"
            fixture.write_text(json.dumps(inverse), encoding="ascii")
        lua = out / "probe.lua"
        lua.write_text(LUA, encoding="ascii")
        env = {key: value for key, value in os.environ.items() if not key.upper().startswith("COINLINE_")}
        env.update(COINLINE_EMU_ROOT=str(out), COINLINE_MOTHERBOARD="later",
                   COINLINE_BOARD=str(ROOT / "fixtures/board/board-profile-2line-vfd.json"), COINLINE_FIRMWARE=str(seed),
                   COINLINE_DIAGNOSTICS="0", COINLINE_TP8048_ROM=str(args.controller.resolve()),
                   COINLINE_SMART_CARD=str(fixture), COINLINE_SMART_CARD_TRACE="1", COINLINE_SMART_CARD_LAB_TRACE="1")
        command = [str(args.mame.resolve()), "cl_millennium", "-noreadconfig", "-skip_gameinfo", "-nodebug", "-nolog",
                   "-video", "none", "-sound", "none", "-rompath", str(args.rompath.resolve()),
                   "-nvram_directory", str(out / "nvram"), "-cfg_directory", str(out / "cfg"),
                   "-seconds_to_run", "5", "-autoboot_delay", "0", "-autoboot_script", str(lua)]
        result = subprocess.run(command, cwd=out, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=90)
        log = result.stdout.decode(errors="replace")
        (out / "mame.log").write_text(log, encoding="utf-8")
        if result.returncode or "SMARTCARD_ASYNC_PROBE marker=A5" not in log:
            raise RuntimeError(f"Native {protocol} contact UART probe failed: {out}")
        print(f"Native {protocol} contact UART passed: {out}", flush=True)


if __name__ == "__main__":
    main()
