#!/usr/bin/env python3
"""
Build TP PCD3349A/8048 ROM.

This wrapper tries real assemblers first:
- asl + p2bin (Macro Assembler AS)
- as8048 (if present)
- sdas48 + sdobjcopy (if present)

If none are available, it emits the deterministic placeholder ROM pattern used by
the emulator backend so runs remain reproducible.
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

ROM_SIZE = 4096


def bundled_asl_tools(emu_root: Path) -> tuple[Path, Path] | None:
    """Look for an in-tree ASL/p2bin pair under tools/tp8048/bin/.

    A user may unpack the Macro Assembler AS distribution there to avoid
    polluting PATH. Returns None if not present; the caller will then look
    on PATH and finally fall back to the deterministic placeholder.
    """
    bin_dir = emu_root / "tools" / "tp8048" / "bin"
    asl = bin_dir / "asl.exe"
    p2bin = bin_dir / "p2bin.exe"
    if asl.is_file() and p2bin.is_file():
        return asl, p2bin
    asl2 = bin_dir / "asl"
    p2bin2 = bin_dir / "p2bin"
    if asl2.is_file() and p2bin2.is_file():
        return asl2, p2bin2
    return None


def run(cmd: list[str], cwd: Path) -> int:
    print(f"[tp8048] $ {' '.join(cmd)}")
    proc = subprocess.run(cmd, cwd=str(cwd), check=False)
    return proc.returncode


def write_placeholder_rom(out_path: Path) -> None:
    data = bytearray(ROM_SIZE)
    for i in range(ROM_SIZE):
        data[i] = (i * 13 + 0x72) & 0xFF
    data[0] = 0x01
    data[1] = 0x02
    data[2] = 0x03
    data[3] = 0x00
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(data)
    print(f"[tp8048] wrote placeholder ROM: {out_path}")


def try_asl(src: Path, out_path: Path, emu_root: Path) -> bool:
    cwd = src.parent

    cand: list[tuple[Path, Path]] = []
    bundled = bundled_asl_tools(emu_root)
    if bundled:
        cand.append(bundled)
    asl_p = shutil.which("asl") or shutil.which("asl.exe")
    p2_p = shutil.which("p2bin") or shutil.which("p2bin.exe")
    if asl_p and p2_p:
        cand.append((Path(asl_p), Path(p2_p)))

    if not cand:
        return False

    for asl, p2bin in cand:
        if run([str(asl), "-cpu", "8048", src.name], cwd) != 0:
            continue
        p_file = src.with_suffix(".p")
        if not p_file.exists():
            continue
        if (
            run(
                [str(p2bin), p_file.name, str(out_path.name), "-r", "0x0000-0x0FFF", "-l", "255"],
                cwd,
            )
            != 0
        ):
            continue
        break
    else:
        return False
    built = cwd / out_path.name
    if built.exists():
        out_path.parent.mkdir(parents=True, exist_ok=True)
        if built.resolve() != out_path.resolve():
            out_path.write_bytes(built.read_bytes())
        return True
    return False


def try_as8048(src: Path, out_path: Path) -> bool:
    as8048 = shutil.which("as8048")
    if not as8048:
        return False
    cwd = src.parent
    bin_candidate = src.with_suffix(".bin")
    if run([as8048, "-o", bin_candidate.name, src.name], cwd) != 0:
        return False
    if not bin_candidate.exists():
        return False
    blob = bin_candidate.read_bytes()
    blob = (blob + b"\x00" * ROM_SIZE)[:ROM_SIZE]
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(blob)
    return True


def try_sdas48(src: Path, out_path: Path) -> bool:
    sdas48 = shutil.which("sdas48")
    sdobjcopy = shutil.which("sdobjcopy")
    if not sdas48 or not sdobjcopy:
        return False
    cwd = src.parent
    rel_path = src.name
    if run([sdas48, "-plosgffwy", rel_path], cwd) != 0:
        return False
    rel_obj = src.with_suffix(".rel")
    ihx = src.with_suffix(".ihx")
    if not rel_obj.exists():
        return False
    if run([sdobjcopy, "-I", "ihx", "-O", "binary", str(rel_obj.name), ihx.name], cwd) != 0:
        return False
    if not ihx.exists():
        return False
    blob = ihx.read_bytes()
    blob = (blob + b"\x00" * ROM_SIZE)[:ROM_SIZE]
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(blob)
    return True


def parse_args() -> argparse.Namespace:
    root = Path(__file__).resolve().parents[2]
    default_src = root / "tools" / "tp8048" / "src" / "telephony_subprocessor.asm"
    default_out = root / "firmware" / "telephony_subprocessor.rom"
    p = argparse.ArgumentParser(description="Build TP 8048 ROM image")
    p.add_argument("--src", type=Path, default=default_src, help="ASM source file")
    p.add_argument("--out", type=Path, default=default_out, help="ROM output path")
    p.add_argument(
        "--allow-placeholder",
        action="store_true",
        help="Emit deterministic placeholder ROM if no assembler is installed",
    )
    return p.parse_args()


def main() -> int:
    args = parse_args()
    emu_root = Path(__file__).resolve().parents[2]
    src = args.src.resolve()
    out_path = args.out.resolve()

    if not src.exists():
        print(f"[tp8048] source not found: {src}", file=sys.stderr)
        return 2

    print(f"[tp8048] source: {src}")
    print(f"[tp8048] output: {out_path}")

    if try_asl(src, out_path, emu_root):
        print("[tp8048] built ROM with asl/p2bin")
        return 0
    if try_as8048(src, out_path):
        print("[tp8048] built ROM with as8048")
        return 0
    if try_sdas48(src, out_path):
        print("[tp8048] built ROM with sdas48/sdobjcopy")
        return 0

    if args.allow_placeholder:
        print("[tp8048] no assembler detected; falling back to deterministic placeholder")
        write_placeholder_rom(out_path)
        return 0

    print("[tp8048] no supported 8048 assembler found", file=sys.stderr)
    print("[tp8048] install one of: asl+p2bin, as8048, or sdas48+sdobjcopy", file=sys.stderr)
    print("[tp8048] rerun with --allow-placeholder to emit deterministic fallback ROM", file=sys.stderr)
    return 3


if __name__ == "__main__":
    raise SystemExit(main())
