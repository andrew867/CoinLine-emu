# Audio / supervision — CI plan

## Principles

- **Never** report **pass** on a test labeled **firmware-driven** if the **`coinline-mame.exe`** / **`mamecoinline.exe`** binary was not executed with the pinned firmware image.
- If firmware is absent in CI: **`SKIP`** with stdout reason — exit code convention documented in test harness (**77** typical skip).

## Forbidden conformance paths

The following **do not** satisfy audio/supervision conformance claims:

| Forbidden | Why |
| --------- | --- |
| Browser/Web Audio playback of recorded WAV compared to expectations | No Z180 I/O path |
| Node/Python **simulation** of voiceware state without MAME `device_t` code | Not firmware-driven |
| Scenario runner setting internal route/voiceware flags **without** matching `io-trace` / device traces | Cheats proof |

## Test tiers (rename consistently)

| Tier | What runs | Firmware | May claim firmware behavior? |
| ---- | --------- | -------- | ------------------------------ |
| **A — Device fixture** | In-process GoogleTest calling `millennium_*_device` methods | No | **No** |
| **B — Driver smoke** | Launches MAME with **no** assertion on firmware traces | Optional | **No** |
| **C — Firmware integration** | **`coinline-mame.exe`** + `flash.bin` + trace assertions | **Required** | **Yes** |

## Standard commands (Windows reference)

| Step | Command |
| ---- | ------- |
| Build | `tools/windows/build-mame-coinline.ps1` |
| Run | `tools/windows/run-coinline-emulator.ps1 -FirmwareBinary <path-to-flash.bin> -RunSeconds N` |
| Aggregate test | `tools/windows/test-coinline-emulator.ps1 -FirmwareBinary <path-to-flash.bin>` |

Linux/macOS: invoke equivalent scripts under `tools/` if present; otherwise document MSYS2 MINGW64 as authoritative per [`BUILDING.md`](BUILDING.md).

## Regression suite (Tranche A7)

1. `tools/windows/build-mame-coinline.ps1`
2. Run emulator with pinned firmware (`fixtures/firmware/firmware-hashes.json`).
3. Parse `audio/*.jsonl` for required `event_type` rows.

## Failure artifacts

Upload `build/runs/<stamp>/`, full evidence bundle directory, and **`emu.log`** showing argv including emulator executable path.

## Relation to existing CI

