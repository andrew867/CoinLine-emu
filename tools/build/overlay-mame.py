#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
import argparse
from pathlib import Path
import shutil
import subprocess


ROOT = Path(__file__).resolve().parents[2]


def overlay(mame):
    if not (mame / "COPYING").is_file():
        raise ValueError("Use the MAME checkout identified by config/mame-pinned.ref.")
    git = ["git", "-C", str(mame), "apply"]
    for patch in sorted((ROOT / "mame-overlays/patches").glob("*.patch")):
        data = patch.read_bytes().replace(b"\r\n", b"\n")
        if subprocess.run([*git, "--reverse", "--check"], input=data, capture_output=True).returncode:
            print(f"Applying {patch.name}", flush=True)
            subprocess.run(git, input=data, check=True)

    for source in sorted((ROOT / "src/mame/coinline").iterdir()):
        if source.suffix in (".cpp", ".h"):
            target = mame / source.relative_to(ROOT)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
    for name in ("scripts/target/mame/coinline.lua", "src/mame/coinline.cpp", "src/mame/coinline.lst"):
        target = mame / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(ROOT / "mame-overlays" / name, target)
    shutil.copyfile(ROOT / "src/mame/layout/millennium.lay", mame / "src/mame/layout/millennium.lay")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Apply the CoinLine driver overlay to a MAME checkout")
    parser.add_argument("mame", type=Path)
    overlay(parser.parse_args().mame.resolve())
