# Building CoinLine Terminal Emulator on Windows (MSYS2 + MAME)

This project **does not** vendor the MAME tree inside `` by default. GPL-2.0-or-later MAME sources live in a **separate checkout**; the CoinLine driver under `src/mame/coinline/` is overlaid into that tree and built with the official MSYS2 + MinGW workflow described upstream.

## Prerequisites

1. **MSYS2** installed at `C:\msys64` (or pass a different root to the scripts).
2. **Git for Windows** on `PATH` (used by `bootstrap-msys2-mame.ps1` for clone/fetch).
3. **PowerShell 5.1+** (or PowerShell 7) to run the `.ps1` drivers.
4. A **pinned MAME ref** in `config/mame-pinned.ref` (first non-comment line), e.g. `mame0287`.
5. A **real firmware binary** on disk for runtime validation (never substitute a dummy ROM for acceptance).

## Official references

- [Compiling MAME](https://docs.mamedev.org/initialsetup/compilingmame.html)
- [MAME Windows / tools](https://www.mamedev.org/tools/)

## Environment variables

| Variable | Purpose |
| -------- | ------- |
| `EXTERNAL_MAME_ROOT` | Absolute path to your MAME checkout (default ` %USERPROFILE%\src\mame`). |
| `FIRMWARE_BINARY_PATH` | Absolute path to the compiled terminal firmware image used by real-boot scripts. |
| `FIRMWARE_SOURCE_ROOT` | Firmware **source** tree for `tools/extract-firmware-evidence.ps1` (e.g. firmware source tree checkout). |
| `COINLINE_MAME_EXE` | Override path to `mame.exe` if not using `build/bin/coinline-mame.exe`. |
| `COINLINE_EMU_ROOT` | Set automatically by run scripts to the `coinline-emu` repo root. |

## One-time bootstrap (pacman + clone + pin)

```powershell
cd coinline-emu
.\tools\windows\bootstrap-msys2-mame.ps1
```

Optional: `-SkipPacman` after packages are installed. The script writes `build/mame-version.json` (git commit + describe).

**MSYS2 shell**: scripts drive **`msys2_shell.cmd -ucrt64`** for the compile step. If your machine requires **CLANG64** instead, adjust `build-mame-coinline.ps1` or invoke the equivalent shell manually and run `_build_mame_inner.sh` from that environment.

**Typical pacman packages** (installed by bootstrap): `git`, `make`, `diffutils`, `patch`, `mingw-w64-ucrt-x86_64-toolchain`, `mingw-w64-ucrt-x86_64-python`, `mingw-w64-ucrt-x86_64-SDL2`, `mingw-w64-ucrt-x86_64-SDL2_ttf`. Add Qt/SDL packages if `make` reports missing dependencies.

## Build MAME with the CoinLine driver

```powershell
$env:EXTERNAL_MAME_ROOT = "$env:USERPROFILE\src\mame"
.\tools\windows\build-mame-coinline.ps1
```

This copies `src/mame/coinline/*` into the MAME tree (`scripts/register-millennium-driver.sh`), then runs `make REGENIE=1 SUBTARGET=mame SOURCES=<all src/mame/coinline/*.cpp> -jN`. **You must still ensure** the `src/mame/mame.lst` (or subtarget list) in your MAME tree includes the printed `src/mame/coinline/*.cpp` lines from the register script if Genie does not pick them up automatically—see the script’s stdout on first integration.

Build logs: `build/logs/mame-build.log` (via `tee` inside MSYS).

Output binary: `build/bin/coinline-mame.exe` (copy of the built `mame.exe`).

## Run the emulator (real firmware)

```powershell
.\tools\windows\run-coinline-emulator.ps1 -FirmwareBinary 'D:\roms\millennium\flash.bin'
```

Artifacts land under `build/runs/<timestamp>/` including `boot-trace.jsonl`, `run-summary.json`, and MAME stdout/stderr logs.

**Driver short name**: the MAME `COMP` registration uses system name **`millennium`** (not `coinline_millennium`). The run script invokes `millennium`.

## Validate boot milestones (real trace)

```powershell
.\tools\validate-boot-milestones.ps1 -RunDir '.\build\runs\<timestamp>'
```

## Full orchestrated test

```powershell
.\tools\windows\test-coinline-emulator.ps1 -FirmwareBinary 'D:\roms\millennium\flash.bin'
```

CI without firmware: **do not** treat success as boot validation. Use `-SkipIfNoFirmware` to exit `77` with a clear message; acceptance pipelines must supply a real binary.

## Extract firmware evidence (optional operator checkout)

```powershell
$env:FIRMWARE_SOURCE_ROOT = 'D:\vendor\firmware-checkout'
.\tools\extract-firmware-evidence.ps1
```

Writes `build/generated/*.json` and refreshes ``. Fails if the checkout does not contain the metadata this script expects (see script `--help`).

