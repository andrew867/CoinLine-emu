# MAME coinline subtarget — uPD7759 / upd775x link failure (resolved)

**Last updated:** 2026-05-04  
**MSYS2:** `C:\msys64`, **`MSYSTEM=MINGW64`**, **`cmake`** → `/mingw64/bin/cmake` (verified in `build/logs/mingw64-env-check.log`).

## Make / OS detection (MSYS2 bash)

Invoking **`make` from `bash` without `cmd.exe`** often leaves **`OS` unset**, so MAME’s makefile takes the generic **`uname`** branch and errors (`Unable to detect OS from uname -a: MINGW64_NT-...`). **`tools/windows/_build_mame_inner.sh`** now exports **`OS=Windows_NT`** before **`make`** so the Windows target path is selected (same effect as `build-mame-coinline.ps1`’s environment when launched from certain hosts).

## Symptom

Link errors when building **`SUBTARGET=coinline`** after Voiceware integration:

- Undefined references to **`upd7759_device`** / **`upd775x_device`** (and transitively anything only linked from **`upd7759.cpp`**).

This was a **link-set / genie** issue: the coinline subtarget did not enable the sound.lua gates that compile NEC uPD7759 sources into the `optional` library.

## Root cause

1. **Definitions:** Both **`upd775x_device`** and **`upd7759_device`** live in **`third-party/mame/src/devices/sound/upd7759.h`** / **`upd7759.cpp`** (`DEFINE_DEVICE_TYPE(UPD7759, ...)` in **`upd7759.cpp`**).

2. **Build wiring:** **`third-party/mame/scripts/src/sound.lua`** includes **`upd7759.cpp`**, **`315-5641.cpp`**, etc., only when **`SOUNDS["UPD7759"]`** is true. **`spkrdev.cpp`** is included when **`SOUNDS["SPEAKER"]`** is true ( **`SPEAKER`** device used alongside **`UPD7759`** in **`millennium_state.cpp`**).

3. **Subtree gap:** **`mame-overlays/scripts/target/mame/coinline.lua`** originally set only **`CPUS`** / **`MACHINES`** — **no `SOUNDS[...]`**, so **`upd7759.cpp`** was never compiled for **`SUBTARGET=coinline`**.

## Driver references (repo overlay sources)

- **`millennium_state.h`**, **`millennium_state.cpp`**, **`millennium_voiceware.h`**: `#include "sound/upd7759.h"`, **`UPD7759(...)`**, **`required_device<upd7759_device>`**.

## Fix applied

In **`mame-overlays/scripts/target/mame/coinline.lua`**:

```lua
SOUNDS["SPEAKER"] = true
SOUNDS["UPD7759"] = true
```

Overlay copies this file into **`$MAME_ROOT/scripts/target/mame/coinline.lua`**. Rebuild with **`REGENIE=1`** ( **`make REGENIE=1 SUBTARGET=coinline NOWERROR=1`** — see **`tools/windows/_build_mame_inner.sh`**).

## Build command (canonical)

```bash
./tools/mingw64/build-coinline-mame.sh
```

Equivalent inner step:

```bash
make SHELL=/usr/bin/bash REGENIE=1 SUBTARGET=coinline NOWERROR=1 -j<N>
```

## Evidence

See **`build/generated/mame-upd7759-link-report.json`** and **`build/logs/mame-build.log`**.
