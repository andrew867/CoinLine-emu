# Visual output (“no visual output”) — status

## Why MAME reported “no visual output”

Earlier driver builds did not expose a **`screen_device`** with a **`screen_update`** implementation, so the UI had nothing to composite for the internal “screen” device path.

## What was added

- **`SCREEN`** in `millennium_state::millennium()` — raster **1280×900**, 60 Hz, **`screen_update`** fills a dark background and draws:
  - **Emulator-only status overlay** (title, milestone, cycles, VFD I/O status) — clearly **not** firmware text.
  - **Firmware-driven VFD buffer** from `millennium_vfd_device` when cells update from port **0x60** writes (no synthetic script text).
- **Optional OEM font (`Millennium` / `artwork/Millenft.ttf`)** on Windows via private font add; **Adafruit-derived 5×7 gfxfont** fallback (see `millennium_vfd_gfxfont.h`).
- **Default layout**: `config.set_default_layout(layout_millennium)` with **`src/mame/layout/millennium.lay`** + front PNG **`millennium-terminal-front.png`** under **`-artpath`** (use repo `artwork/`).

## PNG / artwork wiring

- Layout XML: `src/mame/layout/millennium.lay` (synced to MAME `src/mame/layout/` by `overlay-coinline-driver.ps1`).
- Image: `artwork/millennium-terminal-front.png`.
- Run with **`-artpath ./artwork`** (or your `COINLINE_EMU_ROOT\artwork`) so layout elements resolve.

## Screenshot capture

- **`tools/windows/run-screenshot-capture.ps1`** sets **`-snapshot_directory`** under the run folder, **`-artpath`** to `artwork`, and optional GUI capture via **`capture-mame-screenshot.ps1`**.
- **`vfd-trace.jsonl`** is filtered from **`io-trace.jsonl`** for lines tagged **`vfd_data`** or MMU tags (see script).

## Firmware vs emulator labeling

- **Firmware-driven VFD:** requires **`vfd_data`** / port **0x0060** writes in **`io-trace.jsonl`** and visible non-space cells in the VFD model — counts toward **M6**.
- **Emulator-status-only:** overlay lines and empty/spaced VFD grid before firmware writes — **not** M6 proof.
