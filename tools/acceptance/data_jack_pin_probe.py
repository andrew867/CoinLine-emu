#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Exercise only the documented later data-jack monitor and relay outputs."""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

from run import ROOT


PROBE = r'''
local machine = manager.machine
local io = machine.devices[":maincpu"].spaces["io"]
local jack = assert(machine.ioport.ports[":DATAJACKUI"])
local hook = assert(machine.ioport.ports[":KEYMATRIX"])
local plug, loop = assert(jack:field(1)), assert(jack:field(2))
local base, initial_hook
local frame = 0
local function monitor(expected)
    local value = io:read_u8(0x40)
    assert(value & 0x7f == base, "monitor changed unrelated status bits")
    assert((value & 0x80 ~= 0) == expected, "unexpected external loop monitor")
    assert(hook:read() & 0x80000 == initial_hook, "external loop changed handset")
end
emu.register_frame_done(function()
    frame = frame + 1
    if frame == 4 then base = io:read_u8(0x40) & 0x7f; initial_hook = hook:read() & 0x80000; monitor(false); plug:set_value(1) end
    if frame == 7 then monitor(false); loop:set_value(1) end
    if frame == 10 then monitor(true); io:write_u8(0x43, 0xc0) end
    if frame == 13 then monitor(true); io:write_u8(0x43, 0x50) end
    if frame == 16 then monitor(true); io:write_u8(0x43, 0x80) end
    if frame == 19 then monitor(true); io:write_u8(0x43, 0x00); plug:clear_value() end
    if frame == 22 then monitor(false); loop:clear_value() end
    if frame == 25 then monitor(false); print("DATA_JACK_PIN_PROBE passed"); machine:exit() end
end)
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mame", type=Path, required=True)
    parser.add_argument("--rompath", type=Path, default=ROOT / "build/package-inputs/local/roms")
    parser.add_argument("--controller", type=Path, default=ROOT / "firmware/tp_pcd3349a_behavioural.rom")
    parser.add_argument("--output", type=Path, default=ROOT / "build/firmware-acceptance/data-jack-pin-probe")
    args = parser.parse_args()
    out = args.output.resolve() / datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    out.mkdir(parents=True)
    shutil.copytree(ROOT / "fixtures", out / "fixtures")
    image = bytearray(b"\xff" * 1048576)
    image[0xc0000:0xc0004] = b"\xf3\x76\x18\xfd"
    seed = out / "flash.bin"
    seed.write_bytes(image)
    entry = {"version_id": "synthetic-data-jack-probe", "friendly_name": "Synthetic data-jack probe",
             "sha256_hex": hashlib.sha256(image).hexdigest(), "size_bytes": len(image), "motherboard": "later"}
    (out / "fixtures/firmware/firmware-hashes.json").write_text(json.dumps({"entries": [entry]}))
    lua = out / "probe.lua"
    lua.write_text(PROBE)
    env = {key: value for key, value in os.environ.items() if not key.upper().startswith("COINLINE_")}
    env.update(COINLINE_EMU_ROOT=str(out), COINLINE_MOTHERBOARD="later", COINLINE_HARDWARE_PRESET="multipay-v1.3",
               COINLINE_BOARD=str(ROOT / "fixtures/board/board-profile-2line-vfd.json"), COINLINE_FIRMWARE=str(seed),
               COINLINE_DIAGNOSTICS="0", COINLINE_TP8048_ROM=str(args.controller.resolve()),
               COINLINE_DATA_JACK_PROFILE="downloaded", COINLINE_DATA_JACK_TRACE="1")
    command = [str(args.mame.resolve()), "cl_millennium", "-noreadconfig", "-skip_gameinfo", "-nodebug",
               "-nolog", "-video", "none", "-sound", "none", "-rompath", str(args.rompath.resolve()),
               "-nvram_directory", str(out / "nvram"), "-cfg_directory", str(out / "cfg"),
               "-seconds_to_run", "4", "-autoboot_delay", "0", "-autoboot_script", str(lua)]
    result = subprocess.run(command, cwd=out, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=90)
    log = result.stdout.decode(errors="replace")
    (out / "mame.log").write_text(log, encoding="utf-8")
    routes = ["laptop_and_network", "laptop_only", "network_only", "unknown"]
    if result.returncode or "DATA_JACK_PIN_PROBE passed" not in log or any(f"route={route} " not in log for route in routes):
        raise RuntimeError(f"Native data-jack monitor/relay probe failed: {out}")
    (out / "result.json").write_text(json.dumps({"monitor": "pass", "raw_relay_actions": routes,
                                                 "audio_or_data_session": "unqualified"}, indent=2))
    print(f"Native data-jack monitor and raw relay probe passed: {out}")


if __name__ == "__main__":
    main()
