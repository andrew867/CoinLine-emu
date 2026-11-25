#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Measure initial native window geometry for the portable launcher sizing flags."""

import argparse
import ctypes as C
from ctypes import wintypes as W
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

from window_drag_probe import ROOT, Window, WM_CLOSE


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mame", required=True, type=Path)
    parser.add_argument("--rompath", type=Path, default=ROOT / "build/package-inputs/local/roms")
    parser.add_argument("--output", type=Path, default=ROOT / "build/firmware-acceptance/window-scale")
    args = parser.parse_args()
    if os.name != "nt":
        parser.error("Win32 geometry probe")
    out = args.output.resolve() / datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    out.mkdir(parents=True)
    shutil.copytree(ROOT / "fixtures", out / "fixtures")
    image = bytearray(b"\xff" * 1048576)
    image[0xc0000:0xc0003] = b"\xf3\x18\xfe"
    (out / "flash.bin").write_bytes(image)
    (out / "fixtures/firmware/firmware-hashes.json").write_text(json.dumps({"entries": [{
        "version_id": "window-scale-probe", "friendly_name": "Synthetic window geometry probe",
        "sha256_hex": hashlib.sha256(image).hexdigest(), "size_bytes": len(image), "motherboard": "later"}]}))
    env = {key: value for key, value in os.environ.items() if not key.upper().startswith("COINLINE_")}
    env.update(COINLINE_EMU_ROOT=str(out), COINLINE_MOTHERBOARD="later", COINLINE_HARDWARE_PRESET="multipay-v1.3",
               COINLINE_BOARD=str(ROOT / "fixtures/board/board-profile-2line-vfd.json"),
               COINLINE_FIRMWARE=str(out / "flash.bin"), COINLINE_DIAGNOSTICS="0", COINLINE_LINE_PORT="0",
               COINLINE_TP8048_ROM=str(ROOT / "firmware/tp_pcd3349a_behavioural.rom"))
    result = {}
    for label, flags in (("default", ["-nomaximize", "-resolution", "360x850"]),
                         ("scale2", ["-maximize", "-resolution", "720x1700"]),
                         ("maximize", ["-maximize"])):
        folder = out / label
        folder.mkdir()
        command = [str(args.mame.resolve()), "cl_millennium", "-noreadconfig", "-skip_gameinfo", "-nodebug", "-nolog",
                   "-window", *flags, "-video", "opengl", "-sound", "none", "-artpath", str(ROOT / "artwork"),
                   "-rompath", str(args.rompath.resolve()), "-nvram_directory", str(folder / "nvram"),
                   "-cfg_directory", str(folder / "cfg"), "-seconds_to_run", "15"]
        with (folder / "mame.log").open("wb") as log:
            process = subprocess.Popen(command, cwd=folder, env=env, stdout=log, stderr=subprocess.STDOUT)
            try:
                window = Window(process)
                window.api.GetClientRect.argtypes = [W.HWND, C.POINTER(W.RECT)]
                window.api.GetClientRect.restype = W.BOOL
                rect = W.RECT()
                if not window.api.GetClientRect(window.hwnd, C.byref(rect)):
                    raise C.WinError(C.get_last_error())
                result[label] = [rect.right - rect.left, rect.bottom - rect.top]
                window.post(WM_CLOSE)
                if process.wait(timeout=25) != 0:
                    raise RuntimeError(f"{label} window did not close normally")
            finally:
                if process.poll() is None:
                    process.terminate()
                    process.wait(timeout=5)
    result["passed"] = (result["scale2"][1] >= result["default"][1] and
                        result["maximize"][1] >= result["scale2"][1])
    (out / "geometry.json").write_text(json.dumps(result, indent=2) + "\n")
    print(f"Native window geometry: {out}\n{json.dumps(result, indent=2)}")
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
