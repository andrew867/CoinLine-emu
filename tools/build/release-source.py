#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
import argparse
from io import BytesIO
from pathlib import Path
import subprocess
import tarfile


ROOT = Path(__file__).resolve().parents[2]


def add_checkout(output, root, prefix):
    # Archive Git inputs only. Local ROMs, test captures and build outputs are outside the release.
    with subprocess.Popen(["git", "-C", str(root), "archive", f"--prefix={prefix}/", "HEAD"],
                          stdout=subprocess.PIPE) as process:
        with tarfile.open(fileobj=process.stdout, mode="r|") as source:
            for member in source:
                output.addfile(member, source.extractfile(member) if member.isfile() else None)
        if process.wait():
            raise RuntimeError(f"Could not archive {prefix} sources")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Archive the tracked CoinLine and pinned MAME release sources")
    parser.add_argument("mame", type=Path)
    parser.add_argument("archive", type=Path)
    args = parser.parse_args()
    if ROOT != Path(subprocess.check_output(["git", "-C", str(ROOT), "rev-parse", "--show-toplevel"],
                                            text=True).strip()).resolve():
        parser.error("Use a standalone public emulator checkout to archive release sources.")
    with tarfile.open(args.archive, "x:gz", compresslevel=5) as output:
        add_checkout(output, ROOT, "coinline")
        add_checkout(output, args.mame.resolve(), "mame")
        sha = subprocess.check_output(["git", "-C", str(args.mame), "rev-parse", "HEAD"])
        revision = tarfile.TarInfo("mame/.coinline-upstream-revision")
        revision.size = len(sha)
        output.addfile(revision, BytesIO(sha))
