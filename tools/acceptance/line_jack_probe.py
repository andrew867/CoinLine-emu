#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Verify the real native loopback jack exchanges its initial line signals."""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import time

from run import ROOT, port


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mame", type=Path, required=True)
    parser.add_argument("--rompath", type=Path, default=ROOT / "build/package-inputs/local/roms")
    parser.add_argument("--controller", type=Path, default=ROOT / "firmware/tp_pcd3349a_behavioural.rom")
    parser.add_argument("--output", type=Path, default=ROOT / "build/firmware-acceptance/line-jack-probe")
    args = parser.parse_args()
    out = args.output.resolve() / datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    out.mkdir(parents=True)
    shutil.copytree(ROOT / "fixtures", out / "fixtures")
    image = bytearray(b"\xff" * 1048576)
    image[0xc0000:0xc0004] = b"\xf3\x76\x18\xfd"
    seed = out / "flash.bin"
    seed.write_bytes(image)
    entry = {"version_id": "synthetic-line-jack-probe", "friendly_name": "Synthetic line-jack probe",
             "sha256_hex": hashlib.sha256(image).hexdigest(), "size_bytes": len(image), "motherboard": "later"}
    (out / "fixtures/firmware/firmware-hashes.json").write_text(json.dumps({"entries": [entry]}))
    line_port = int(port())
    env = {key: value for key, value in os.environ.items() if not key.upper().startswith("COINLINE_")}
    env.update(COINLINE_EMU_ROOT=str(out), COINLINE_MOTHERBOARD="later", COINLINE_HARDWARE_PRESET="multipay-v1.3",
               COINLINE_BOARD=str(ROOT / "fixtures/board/board-profile-2line-vfd.json"), COINLINE_FIRMWARE=str(seed),
               COINLINE_DIAGNOSTICS="0", COINLINE_TP8048_ROM=str(args.controller.resolve()), COINLINE_LINE_PORT=str(line_port))
    command = [str(args.mame.resolve()), "cl_millennium", "-noreadconfig", "-skip_gameinfo", "-nodebug",
               "-nolog", "-video", "none", "-sound", "none", "-rompath", str(args.rompath.resolve()),
               "-nvram_directory", str(out / "nvram"), "-cfg_directory", str(out / "cfg"), "-seconds_to_run", "6"]
    data = b""
    with (out / "mame.log").open("wb") as log:
        process = subprocess.Popen(command, cwd=out, env=env, stdout=log, stderr=subprocess.STDOUT)
        try:
            deadline = time.monotonic() + 15
            while True:
                try:
                    sock = socket.create_connection(("127.0.0.1", line_port), timeout=.2)
                    break
                except OSError:
                    if process.poll() is not None or time.monotonic() >= deadline:
                        raise RuntimeError(f"Native jack did not listen: {out}")
                    time.sleep(.05)
            with sock:
                sock.settimeout(2)
                sock.sendall(b"C1\n")
                while len(data) < 4096:
                    try:
                        chunk = sock.recv(4096)
                    except socket.timeout:
                        break
                    if not chunk:
                        break
                    data += chunk
                    if b"H0\n" in data and b"Q0\n" in data and (b"E0\n" in data or b"E1\n" in data):
                        break
            process.wait(timeout=20)
        finally:
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=5)
    signals = [line for line in data.decode("ascii", "replace").splitlines() if not line.startswith("M")]
    passed = all(signal in signals for signal in ("H0", "Q0")) and any(s in signals for s in ("E0", "E1"))
    (out / "result.json").write_text(json.dumps({"signals": signals, "passed": passed}, indent=2))
    print(f"Native line-jack {'passed' if passed else 'failed'}: {out}; signals={signals}")
    if not passed:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
