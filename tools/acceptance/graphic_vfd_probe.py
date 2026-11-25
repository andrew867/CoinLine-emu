#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Native synthetic test of the graphical VFD CPU bus and physical inputs."""
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
sys.path.insert(0, str(ROOT / "tools/vfd"))
from bmp2vfd import PackedGraphic, Raster, pack_raster, preview_png, unpack_graphic


def pattern():
    # Distinct top and bottom, asymmetric corners and paired side annotations.
    rows = tuple(tuple(int(
        (x in (0, 125) and 0 <= y < 88) or (y in (0, 87) and 0 <= x < 126)
        or (3 <= x < 45 and 3 <= y < 8)
        or (7 <= y < 49 and x == y + 17)
        or (65 <= x <= 116 and y in (21, 40))
        or (x in (65, 116) and 21 <= y <= 40)
        or (y // 8 in (6, 8, 10) and y % 8 in (1, 6)
            and (4 <= x < 28 or 92 <= x < 121))
    ) for x in range(126)) for y in range(88))
    return pack_raster(Raster(126, 88, rows), "page")


def synthetic_image(page):
    program = bytearray(b"\xf3")  # DI

    def out(port, value):
        # LD BC, port; LD A, value; OUT (C),A
        program.extend((0x01, port & 255, port >> 8, 0x3e, value, 0xed, 0x79))

    out(0xf0, 0)
    for index in range(4):
        out(0x80, 0x33 | ((index & 2) << 5))
        base = 0x1280 + (index & 1) * 0x800
        if index == 1:
            for offset, value in enumerate(page):
                if value:
                    out(base + offset, value)
        else:
            for offset in (0, 0x7f, 0x80, 0x500, 0x57f):
                out(base + offset, index + 1)
    # Clear page 2, preserving the other three. Establish the low select bit.
    out(0x80, 0x73)
    out(0x1280, 0x55)
    out(0x80, 0x72)
    out(0x80, 0x73)
    # Display page 1; brightness access must not overwrite its base byte.
    out(0x80, 0x35)
    out(0x1a80, 0x03)
    out(0x80, 0x37)
    program.extend((0x76, 0x18, 0xfd))
    image = bytearray(b"\xff" * 1048576)
    image[0xc0000:0xc0000 + len(program)] = program
    return bytes(image)


PROBE_LUA = """
local machine = manager.machine
local space = machine.devices[":maincpu"].spaces["io"]
local keys = machine.ioport.ports[":TERMINAL21_SOFTKEYS"]
local names = {"SK top left", "SK mid left", "SK bot left", "SK top right",
    "SK mid right", "SK bot right", "SK up", "SK down", "SK left", "SK right",
    "SK spare 1", "SK spare 2"}
local frame = 0
local function scan(key)
    for group = 0, 2 do
        space:write_u8(0x90, 1 << group)
        local actual = space:read_u8(0x90) & 0x78
        local expected = 0x78
        if key >= 0 and key % 3 == group then expected = expected & ~(1 << (key // 3 + 3)) end
        assert(actual == expected, string.format("key %d group %d returned %02X", key, group, actual))
    end
end
emu.register_frame_done(function()
    frame = frame + 1
    if frame == 5 then
        assert(space:read_u8(0x80) & 1 == 1, "display not ready")
        scan(-1)
    end
    if frame >= 6 and frame < 30 then
        local key = (frame - 6) // 2
        local field = assert(keys.fields[names[key + 1]])
        if frame % 2 == 0 then field:set_value(1); scan(key); scan(key)
        else field:set_value(0); scan(-1) end
    end
    if frame == 31 then
        keys.fields[names[1]]:set_value(1)
        keys.fields[names[5]]:set_value(1)
        space:write_u8(0x90, 1)
        assert(space:read_u8(0x90) & 0x78 == 0x70)
        space:write_u8(0x90, 2)
        assert(space:read_u8(0x90) & 0x78 == 0x68)
        keys.fields[names[1]]:set_value(0)
        keys.fields[names[5]]:set_value(0)
        scan(-1)
    end
    if frame == 40 then
        machine.screens[":screen"]:snapshot("probe.png")
        print("GRAPHIC_VFD_PROBE passed")
        machine:exit()
    end
end, "graphical VFD test")
"""


def verify_native_pixels(path, page):
    try:
        from PIL import Image
    except ImportError:
        print("Pillow unavailable; native PNG pixel comparison skipped", flush=True)
        return
    raster = unpack_graphic(PackedGraphic(page, 126, 128, 11))
    with Image.open(path) as source:
        image = source.convert("RGB")
        if image.size != (1280, 900):
            raise RuntimeError("Unexpected native graphical snapshot geometry")
        for y, row in enumerate(raster.rows):
            for x, expected in enumerate(row):
                red, green, blue = image.getpixel((15 + 10 * x, 15 + 10 * y))
                if (green > 150 and red < 100 and blue > 100) != bool(expected):
                    raise RuntimeError(f"Native raster pixel mismatch at {x},{y}")
    print("Native raster: all 11088 visible dot positions match", flush=True)


def run(args, root, page, image, graphical):
    directory = root / ("artwork" if graphical else "headless")
    directory.mkdir()
    shutil.copytree(ROOT / "fixtures", directory / "fixtures")
    digest = hashlib.sha256(image).hexdigest()
    seed = directory / "flash.bin"
    seed.write_bytes(image)
    entry = {"version_id": "synthetic-graphic-vfd", "friendly_name": "Synthetic display test",
             "sha256_hex": digest, "size_bytes": len(image), "motherboard": "later"}
    (directory / "fixtures/firmware/firmware-hashes.json").write_text(json.dumps({"entries": [entry]}))
    script = directory / "probe.lua"
    script.write_text(PROBE_LUA)
    artwork = directory / "panel-art/cl_millennium"
    artwork.mkdir(parents=True)
    shutil.copyfile(ROOT / "src/mame/layout/millennium.lay", artwork / "default.lay")
    shutil.copyfile(ROOT / "artwork/millennium-terminal-front.png", artwork / "millennium-terminal-front.png")
    env = {key: value for key, value in os.environ.items() if not key.upper().startswith("COINLINE_")}
    env.update(COINLINE_EMU_ROOT=str(directory), COINLINE_MOTHERBOARD="later",
               COINLINE_BOARD=str(ROOT / "fixtures/board/board-profile-11line-vfd.json"),
               COINLINE_FIRMWARE=str(seed), COINLINE_DIAGNOSTICS="0",
               COINLINE_TP8048_ROM=str(args.controller.resolve()),
               COINLINE_GRAPHIC_VFD_TRACE=str(directory / "bus.jsonl"),
               COINLINE_GRAPHIC_VFD_SNAPSHOT=str(directory / "pages.json"))
    result = subprocess.run([
        str(args.mame.resolve()), "cl_millennium", "-noreadconfig", "-skip_gameinfo", "-nodebug",
        "-nolog", "-video", "gdi" if graphical and os.name == "nt" else "none", "-sound", "none",
        "-rompath", str(args.rompath.resolve()), "-artpath", str(artwork.parent),
        "-nvram_directory", str(directory / "nvram"), "-cfg_directory", str(directory / "cfg"),
        "-snapshot_directory", str(directory), "-snapview", "Graphical" if graphical else "native",
        "-view", "Graphical", "-window", "-nomaximize", "-seconds_to_run", "4",
        "-autoboot_delay", "0", "-autoboot_script", str(script)
    ], cwd=directory, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=90)
    log = result.stdout.decode(errors="replace")
    (directory / "mame.log").write_text(log, encoding="utf-8")
    if result.returncode or "GRAPHIC_VFD_PROBE passed" not in log:
        raise RuntimeError(f"Native probe failed: {directory}")
    snapshot = json.loads((directory / "pages.json").read_text())
    pages = [bytes.fromhex(value) for value in snapshot["pages_hex"]]
    expected = [bytearray(1408), bytearray(page), bytearray(1408), bytearray(1408)]
    for index in (0, 3):
        for offset in (0, 0x7f, 0x80, 0x500, 0x57f):
            expected[index][offset] = index + 1
    if pages != expected or snapshot["visible_page"] != 1 or snapshot["brightness"] != 3:
        raise RuntimeError(f"Native page state mismatch: {directory}")
    trace = [json.loads(line) for line in (directory / "bus.jsonl").read_text().splitlines()]
    if not all(any(event["port"] == p and event["direction"] == "w" for event in trace)
               for p in (0x1280, 0x1a80, 0x17ff, 0x1fff)):
        raise RuntimeError(f"Native full-address I/O absent: {directory}")
    if not (directory / "probe.png").is_file():
        raise RuntimeError(f"Native snapshot missing: {directory}")
    if not graphical:
        verify_native_pixels(directory / "probe.png", page)
    print(f"{directory.name}: page bytes and twelve physical inputs verified", flush=True)
    return snapshot


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mame", type=Path, required=True)
    parser.add_argument("--rompath", type=Path, default=ROOT / "build/package-inputs/local/roms")
    parser.add_argument("--controller", type=Path, default=ROOT / "firmware/tp_pcd3349a_behavioural.rom")
    parser.add_argument("--output", type=Path, default=ROOT / "build/firmware-acceptance/graphic-vfd-probe")
    parser.add_argument("--artwork", action="store_true", help="also test the artwork view and native window on Windows")
    args = parser.parse_args()
    directory = args.output.resolve() / datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    directory.mkdir(parents=True)
    print(directory, flush=True)
    graphic = pattern()
    (directory / "expected.png").write_bytes(preview_png(graphic))
    image = synthetic_image(graphic.data)
    headless = run(args, directory, graphic.data, image, False)
    if args.artwork:
        if run(args, directory, graphic.data, image, True) != headless:
            raise RuntimeError("Artwork and headless hardware states differ")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
