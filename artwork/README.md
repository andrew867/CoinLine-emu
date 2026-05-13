# `artwork/`

This directory holds the MAME `.lay` artwork and the front-panel image used by the emulator.

## Contents

| File | Purpose |
| ---- | ------- |
| `Millenft.ttf` | TrueType font whose face name is **Millennium** (internal metadata). Used on Windows for firmware-driven VFD text when `millennium_state` loads this file via `AddFontResourceEx` (`FR_PRIVATE`). Ensure `COINLINE_EMU_ROOT` points at the emu repo so `artwork/Millenft.ttf` resolves. Other hosts fall back to an Adafruit-derived 5×7 gfxfont (BSD-3-Clause) from `millennium_vfd_gfxfont.h`. |
| `millennium-terminal-front.png` | Front-panel reference image for layouts / screenshots. |
| `millennium.lay` | MAME layout (regions per [`../docs/artwork-and-layout.md`](../docs/artwork-and-layout.md)). |

## Planned polish

| Item | Notes |
| ---- | ----- |
| Layout wiring | `millennium.lay` + `-artpath` integration with the raster screen device (see driver notes). |

## Authoring rules

- Artwork files are part of the emulator's distribution and inherit the engine track's license posture. - The front-panel image must support both the 2-line VFD variant (default) and the 11-line VFD variant (selected by board profile). The large black display area in the reference image accommodates both.
- The `.lay` file references regions by name; coordinates use MAME's normalized coordinate system.

## Cross-references

- [`../docs/artwork-and-layout.md`](../docs/artwork-and-layout.md).
- [`../docs/mame-driver-plan.md`](../docs/mame-driver-plan.md).
- [`../test-plans/artwork-layout-tests.md`](../test-plans/artwork-layout-tests.md).
