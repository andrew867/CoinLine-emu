# Millennium artwork + layout (driver notes)

- **Layout source (repo):** `src/mame/layout/millennium.lay` — copied into the MAME tree by `tools/windows/overlay-coinline-driver.ps1` → `third-party/mame/src/mame/layout/millennium.lay`. REGENIE builds **`millennium.lh`**; **`millennium_state.cpp`** includes **`millennium.lh`** and calls **`config.set_default_layout(layout_millennium)`**.
- **Raster fallback:** always active via **`screen_update`** (driver works without artwork).
- **Art path:** run MAME with **`-artpath`** set to the repo **`artwork`** directory (for example with `COINLINE_EMU_ROOT` set, pass that path + `\artwork`) so **`millennium-terminal-front.png`** loads for the layout “front” element.
- **VFD:** firmware draws through port **0x60** into `millennium_vfd_device`; the layout does not replace CPU-driven pixels — it is a bezel/reference layer.
