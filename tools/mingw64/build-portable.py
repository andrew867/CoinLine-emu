#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
import argparse
from datetime import datetime, timezone
import hashlib
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import tarfile
from urllib.parse import urlsplit
from zipfile import ZIP_DEFLATED, ZipFile


ROOT = Path(__file__).resolve().parents[2]
PORTABLE = ROOT / "tools/windows/portable"
PYTHON = (
    "https://www.python.org/ftp/python/3.13.17/python-3.13.17-embed-amd64.zip",
    "9e35319bffedab8b36ad44d8f377cb4f1733bcec257a0b6d04d94f01486ddb38",
)
SIP_WHEELS = (
    (
        "https://files.pythonhosted.org/packages/6b/0c/c4ab3c8d5c9e392d5342f7268bc35c3b28ae28a8c4a4c2fdadb64d4fe561/pyVoIP-1.6.8-py3-none-any.whl",
        "ef9aa4518a888dc4523e76e36ac8cb2222b2131672eec8fe6f8d225caeed1faf",
    ),
    (
        "https://files.pythonhosted.org/packages/34/a5/9d78fdb5b844a83da8a71226c7bdae7cc638861085fff7a1d707cb4823fa/audioop_lts-0.2.2-cp313-abi3-win_amd64.whl",
        "a2c2a947fae7d1062ef08c4e369e0ba2086049a5e598fda41122535557012e9e",
    ),
    (
        "https://files.pythonhosted.org/packages/c3/70/43db21af194580aba2d9a6d4c7bd8c1a6e887fa52cd810b88f89096ecad2/pylibsrtp-1.0.0-cp310-abi3-win_amd64.whl",
        "8d6527c4a78a39a8d397f8862a8b7cdad4701ee866faf9de4ab8c70be61fd34d",
    ),
    (
        "https://files.pythonhosted.org/packages/37/18/6519e1ee6f5a1e579e04b9ddb6f1676c17368a7aba48299c3759bbc3c8b3/cffi-2.0.0-cp313-cp313-win_amd64.whl",
        "19f705ada2530c1167abacb171925dd886168931e0a7b78f5bffcae5c6b5be75",
    ),
    (
        "https://files.pythonhosted.org/packages/a0/e3/59cd50310fc9b59512193629e1984c1f95e5c8ae6e5d8c69532ccc65a7fe/pycparser-2.23-py3-none-any.whl",
        "e5c6e8d3fbad53479cab09ac03729e0a9faf2bee3db8208a550daf5af81a5934",
    ),
)
PRIVATE_ROMS = {
    "firmware/oem/flash.bin": 0x40000,
    "roms/cl_millennium/voice_u16.bin": 0x100000,
    "roms/cl_millennium/voice_u26.bin": 0x100000,
}


def sha256(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def output(command):
    return subprocess.check_output(command, text=True).strip()


def copy_file(source, destination):
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, destination)


def fetch(dependency, cache, work):
    url, digest = dependency
    path = cache / Path(urlsplit(url).path).name
    if not path.is_file():
        download = work / path.name
        subprocess.run(["curl", "--fail", "--silent", "--show-error", "--location", "--retry", "2",
                        "--output", str(download), url], check=True)
        if sha256(download) != digest:
            raise ValueError(f"Downloaded SHA-256 did not match for {path.name}")
        copy_file(download, path)
    if sha256(path) != digest:
        raise ValueError(f"Cached SHA-256 did not match for {path.name}")
    return path


def build_mame(mame, work, windows):
    if not (mame / "COPYING").is_file():
        raise ValueError("Set --mame-root to the MAME checkout identified by config/mame-pinned.ref.")
    jobs = str(min(os.cpu_count() or 4, 6))
    subprocess.run([sys.executable, str(ROOT / "tools/build/overlay-mame.py"), str(mame)], check=True)
    if not windows:
        # Disable optional libraries so the artifact uses the same SDL dependencies on both platforms.
        subprocess.run(["make", "REGENIE=1", "SUBTARGET=coinline", "NOWERROR=1", "USE_QTDEBUG=0",
                        "NO_USE_PORTAUDIO=1", "NO_USE_PULSEAUDIO=1", "USE_PCAP=0", "USE_TAPTUN=0",
                        "-j", jobs], cwd=mame, check=True)
        return mame / "mamecoinline"
    env = dict(os.environ, COINLINE_EMU_ROOT=str(ROOT), EXTERNAL_MAME_ROOT=str(mame),
               COINLINE_MAME_BUILD_LOG=str(work / "mame-build.log"),
               COINLINE_MAKE_JOBS=jobs)
    subprocess.run(["bash", str(ROOT / "tools/windows/_build_mame_inner.sh")], env=env, check=True)
    return mame / "mamecoinline.exe"


def include_mame(source, target):
    system32 = Path(os.environ.get("SystemRoot", "C:/Windows")) / "System32"
    mingw_bin = Path(output(["cygpath", "-w", "/mingw64/bin"]))
    libraries = {path.name.lower(): path for folder in (mingw_bin, source.parent)
                 for path in folder.glob("*.dll")}
    system = {path.name.lower() for path in system32.glob("*.dll")}
    copied = set()
    queue = [(source, target)]
    while queue:
        executable, destination = queue.pop()
        if destination.name.lower() in copied:
            continue
        copy_file(executable, destination)
        copied.add(destination.name.lower())
        pe = output([str(mingw_bin / "objdump.exe"), "-p", str(executable)])
        for name in re.findall(r"DLL Name:\s*(\S+)", pe):
            name = name.lower()
            if name in system or name.startswith(("api-ms-win-", "ext-ms-win-")):
                continue
            if name not in libraries:
                raise ValueError(f"Missing native dependency {name} for {executable.name}")
            queue.append((libraries[name], target.parent / name))


def stage_runtime(bundle, cache, work):
    runtime = bundle / "runtime"
    with ZipFile(fetch(PYTHON, cache, work)) as archive:
        archive.extractall(runtime)
    packages = runtime / "Lib/site-packages"
    for wheel in SIP_WHEELS:
        with ZipFile(fetch(wheel, cache, work)) as archive:
            archive.extractall(packages)
    pth = runtime / "python313._pth"
    pth.write_text(pth.read_text(encoding="ascii") + "\nLib/site-packages\n../app\n", encoding="ascii")


def stage_files(bundle, mame, rom_input, windows):
    sources = {
        "Quick Start.txt": PORTABLE / ("quick-start.txt" if windows else "quick-start-posix.txt"),
        "coinline.example.ini": PORTABLE / "coinline.example.ini",
        "app/launch.py": PORTABLE / "launch.py",
        "app/hardware.py": PORTABLE / "hardware.py",
        "app/central_office.py": ROOT / "tools/line-jack/central_office.py",
        "app/sip_media.py": ROOT / "tools/line-jack/sip_media.py",
        "artwork/Millenft.ttf": ROOT / "artwork/Millenft.ttf",
        "artwork/cl_millennium/default.lay": ROOT / "src/mame/layout/millennium.lay",
        "artwork/cl_millennium/millennium-terminal-front.png": ROOT / "artwork/millennium-terminal-front.png",
        "firmware/tp_pcd3349a_behavioural.rom": ROOT / "firmware/tp_pcd3349a_behavioural.rom",
        "fixtures/board/board-profile-2line-vfd.json": ROOT / "fixtures/board/board-profile-2line-vfd.json",
        "fixtures/board/board-profile-11line-vfd.json": ROOT / "fixtures/board/board-profile-11line-vfd.json",
        "fixtures/board/io-port-map.json": ROOT / "fixtures/board/io-port-map.json",
        "fixtures/cards/magcard-valid.json": ROOT / "fixtures/cards/magcard-valid.json",
        "fixtures/cards/smartcard-valid.json": ROOT / "fixtures/cards/smartcard-valid.json",
        "fixtures/cards/smartcard-sync-test.json": ROOT / "fixtures/cards/smartcard-sync-test.json",
        "fixtures/display/vfd-2line-idle.json": ROOT / "fixtures/display/vfd-2line-idle.json",
        "fixtures/firmware/firmware-hashes.json": ROOT / "fixtures/firmware/firmware-hashes.json",
        "docs/Verification.md": ROOT / "docs/status/ncc-install-firmware-stx-handoff-2026-11-23.md",
        "docs/status/smartcard-contact-2026-11-25.md": ROOT / "docs/status/smartcard-contact-2026-11-25.md",
        "docs/status/adsi-interop-2026-11-25.md": ROOT / "docs/status/adsi-interop-2026-11-25.md",
        "docs/status/graphical-render-performance-2026-11-25.md": ROOT / "docs/status/graphical-render-performance-2026-11-25.md",
        "docs/status/peripheral-handoff-implementation-2026-11-25.md": ROOT / "docs/status/peripheral-handoff-implementation-2026-11-25.md",
        "docs/Running.md": ROOT / "RUNNING.md",
        "licenses/CoinLine.txt": ROOT / "LICENSE",
        "licenses/MAME.txt": mame / "COPYING",
    }
    if rom_input:
        sources.update({name: rom_input / name for name in PRIVATE_ROMS})
    for destination, source in sources.items():
        copy_file(source, bundle / destination)
    if windows:
        script = ('@echo off\n"%~dp0runtime\\python.exe" "%~dp0app\\launch.py" oem %*\n'
                  'set "result=%errorlevel%"\nif not "%result%"=="0" pause\nexit /b %result%\n')
        (bundle / "Start OEM.cmd").write_text(script, encoding="ascii", newline="\r\n")
    else:
        launcher = bundle / "start-oem.sh"
        launcher.write_text('#!/bin/sh\ncd -- "$(dirname -- "$0")" || exit 1\nexec python3 app/launch.py oem "$@"\n',
                            encoding="ascii")
        launcher.chmod(0o755)


def pack(args):
    if not (ROOT / "firmware/tp_pcd3349a_behavioural.rom").is_file():
        raise ValueError("Assemble the telephony controller first. See tools/tp8048/README.md.")
    if args.rom_input:
        for name, size in PRIVATE_ROMS.items():
            file = args.rom_input / name
            if not file.is_file() or file.stat().st_size != size:
                raise ValueError(f"Supply {name} with exactly {size} bytes under --rom-input.")

    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    suffix = "-local-roms" if args.rom_input else ""
    windows = os.environ.get("MSYSTEM") == "MINGW64"
    os_label = "Windows" if windows else {"darwin": "macOS", "linux": "Linux"}[sys.platform]
    cpu = "x64" if windows else {"x86_64": "x64", "aarch64": "arm64", "arm64": "arm64"}[platform.machine()]
    name = f"CoinLine-Emulator-OEM-{os_label}-{cpu}-{stamp}{suffix}"
    args.output.mkdir(parents=True, exist_ok=True)
    args.cache.mkdir(parents=True, exist_ok=True)
    work = args.output / (name + "-work")
    bundle = work / "CoinLine Emulator"
    bundle.mkdir(parents=True)
    executable = args.mame_exe or build_mame(args.mame_root, work, windows)
    if windows:
        include_mame(executable, bundle / "bin/coinline-mame.exe")
        stage_runtime(bundle, args.cache, work)
    else:
        target = bundle / "bin/coinline-mame"
        copy_file(executable, target)
        target.chmod(0o755)
    stage_files(bundle, args.mame_root, args.rom_input, windows)

    source_pin = args.mame_root / ".coinline-upstream-revision"
    revision = source_pin.read_text(encoding="ascii").strip() if source_pin.is_file() else output(
        ["git", "-C", str(args.mame_root), "rev-parse", "HEAD"])
    (bundle / "Package.txt").write_text(
        f"{os_label} {cpu}, OEM firmware profile\nNative MAME base revision: {revision}\n"
        f"Local ROM inputs included: {'yes' if args.rom_input else 'no'}\n"
        + ("CPython 3.13.17, pyVoIP 1.6.8, audioop-lts 0.2.2, pylibsrtp 1.0.0\n" if windows
           else "Local CO only. See Quick Start.txt for the system runtime dependencies.\n"),
        encoding="ascii",
    )
    entries = sorted(path for path in bundle.rglob("*") if path.is_file())
    manifest = bundle / "SHA256SUMS.txt"
    manifest.write_text("".join(f"{sha256(path)}  {path.relative_to(bundle).as_posix()}\n" for path in entries),
                        encoding="ascii")
    archive_path = args.output / (name + (".zip" if windows else ".tar.gz"))
    if windows:
        with ZipFile(archive_path, "x", compression=ZIP_DEFLATED, compresslevel=5) as archive:
            for path in (*entries, manifest):
                archive.write(path, path.relative_to(work).as_posix())
    else:
        with tarfile.open(archive_path, "x:gz", compresslevel=5) as archive:
            for path in (*entries, manifest):
                archive.add(path, arcname=path.relative_to(work).as_posix())
    archive_path.with_name(archive_path.name + ".sha256").write_text(
        f"{sha256(archive_path)}  {archive_path.name}\n", encoding="ascii")
    print(archive_path)


def main():
    parser = argparse.ArgumentParser(description="Build and package the OEM emulator")
    parser.add_argument("--mame-root", type=Path, default=ROOT / "third-party/mame", help="prepared MAME checkout")
    parser.add_argument("--mame-exe", type=Path, help="reuse a specific build instead of rebuilding")
    parser.add_argument("--rom-input", type=Path, help="bundle operator-supplied ROMs from the package directory layout")
    parser.add_argument("--cache", type=Path, default=ROOT / "build/package-cache")
    parser.add_argument("--output", type=Path, default=ROOT / "build/dist")
    args = parser.parse_args()
    if os.environ.get("MSYSTEM") != "MINGW64" and sys.platform not in ("linux", "darwin"):
        parser.error("On Windows, run from MSYS2 MINGW64.")
    for field in ("mame_root", "mame_exe", "rom_input", "cache", "output"):
        if getattr(args, field) is not None:
            setattr(args, field, getattr(args, field).resolve())
    try:
        pack(args)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"Packaging failed: {error}\n")


if __name__ == "__main__":
    main()
