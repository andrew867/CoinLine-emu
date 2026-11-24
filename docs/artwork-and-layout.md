# Artwork and layout

The [portable Windows ZIP](../BUILDING.md) stages its MAME layout and front-panel PNG together under `artwork/cl_millennium/`. The launcher supplies the artwork path and starts a window by default. Use the [keyboard controls](../RUNNING.md#controls) for the INSTALL sequence.

This document specifies the MAME `.lay` artwork file and how it is composed against the supplied terminal front-panel image.

## Inputs

| Input | Source | Target |
| ----- | ------ | ------ |
| Front-panel image | `docs/images/terminal-unbranded.webp` | `artwork/millennium-terminal-front.png` |
| MAME `.lay` file | [layout generator](../tools/layout/generate_millennium_layout.py) | `artwork/millennium.lay` and `src/mame/layout/millennium.lay` |

The image at `docs/images/terminal-unbranded.webp` is the base reference. The large black display area accommodates the **2-line VFD** (the common variant) and the **optional 11-line VFD**, selected by the active board profile.

## Layout structure

The `.lay` file declares:

- A background bitmap layer (`millennium-terminal-front.png`).
- One overlay layer per VFD variant (only the active one is shown).
- One clickable input region per front-panel control (see table below).
- Optional indicator overlays (e.g., card slot light, coin return).

## Clickable regions

| Region | MAME element | Behavior | Mapped device |
| ------ | ------------ | -------- | -------------- |
| Handset photo and handset button | `hit_misc`, `handset_button` | Either click toggles the hookswitch. The button shows the action for the current state. | `KEYMATRIX` |
| Numeric keypad | `hit_num` | Click sends 0 through 9, star, or pound | `KEYMATRIX` |
| Quick-access buttons | `hit_quick` | Five buttons, positions 1 through 5 from left to right | `KEYMATRIX` |
| Volume up and down | `hit_up`, `hit_down` | Click changes handset volume | `KEYMATRIX` |
| Language and New Call | `hit_lang`, `hit_newcall` | Click sends the selected keypad input | `KEYMATRIX` |
| Card reader | `hit_misc` | Insert, read, and remove the magstripe test card | `CARDUI` |
| Coin slot | `hit_coin` | Click triggers a coin insertion | `COINUI` |
| Lock | `hit_misc` | Click toggles the cabinet lock | `SECMASK` |

Each region's `inputtag` and `inputmask` attributes connect it to a device input. The five panel buttons use the same positions as the first five inputs on the ten-button profile. Service and line-test switches are keyboard controls.

The lower-left LED panel reads the housing, lock, and coin-vault switches. Green shows closed or locked. Amber shows open or unlocked. Click anywhere in a row to toggle the corresponding switch.

## Display overlays

Two overlays are declared:

| Overlay | Variant | Activation |
| ------- | ------- | ---------- |
| `vfd_2line` | 2-line | Active when `display.variant = "2line"` |
| `vfd_11line` | 11-line | Active when `display.variant = "11line"` |

The overlays render the VFD device's text buffer at the corresponding screen location. Coordinates are calibrated against the front-panel image dimensions and pinned in the `.lay`.

## Authoring guidelines

- Coordinates use MAME's normalized coordinate system (`<bounds x="..." y="..." width="..." height="..."/>`).
- Region names are kebab-case in the `.lay` element ids.
- Each region is documented in `tests/integration/test_artwork_regions.cpp` with a coordinate sanity check.

## Screenshot evidence

Evidence bundles include screenshots at every test step that asserts a visual state. Screenshot tests:

- `tests/integration/test_artwork_idle.cpp` — boot-to-idle screenshot.
- `tests/integration/test_artwork_card_swipe.cpp` — during a card swipe.
- `tests/integration/test_artwork_service_mode.cpp` — service-mode entry.

Screenshots are PNGs saved under the evidence bundle's `screenshots/` subdirectory.

## Substitute artwork

If `docs/images/terminal-unbranded.webp` is not provided, the emulator runs headless and produces only programmatic VFD buffer captures. The `.lay` file still references `millennium-terminal-front.png`; if the file is absent, MAME shows a placeholder background.

## Cross-references

- [`mame-driver-plan.md`](mame-driver-plan.md) — input port declarations.
- [`vfd-emulation.md`](vfd-emulation.md) — VFD overlays.
- [`keypad-emulation.md`](keypad-emulation.md), [`hookswitch-and-handset.md`](hookswitch-and-handset.md), [`card-reader-emulation.md`](card-reader-emulation.md), [`smartcard-emulation.md`](smartcard-emulation.md), [`coin-validator-emulation.md`](coin-validator-emulation.md), [`lock-door-vault-service.md`](lock-door-vault-service.md).
- [`../test-plans/artwork-layout-tests.md`](../test-plans/artwork-layout-tests.md).
