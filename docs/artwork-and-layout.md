# Artwork and layout

This document specifies the MAME `.lay` artwork file and how it is composed against the supplied terminal front-panel image.

## Inputs

| Input | Source | Target |
| ----- | ------ | ------ |
| Front-panel image | `docs/images/terminal-unbranded.webp` | `artwork/millennium-terminal-front.png` |
| MAME `.lay` file | authored | `artwork/millennium.lay` |

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
| Handset / hookswitch | `handset` | Click toggles on-hook / off-hook | `hookswitch` (in keypad device) |
| Numeric keypad (0–9, *, #) | `key_<n>` | Click sends matrix press | keypad |
| Quick-access keys (per profile) | `qa_<name>` | Click sends discrete signal | keypad |
| Volume up | `vol_up` | Click sends discrete signal | keypad |
| Volume down | `vol_down` | Click sends discrete signal | keypad |
| Language | `lang` | Click sends discrete signal | keypad |
| Card slot | `card_slot` | Click triggers magstripe swipe | card |
| Smart-card slot | `smartcard_slot` | Click inserts smart card | smartcard |
| Coin input | `coin_in` | Click triggers coin pulse (denomination from selected fixture) | coin |
| Coin return | `coin_return` | Click triggers return | coin |
| Lock | `lock` | Toggles lock state | security |
| Door | `door` | Toggles door state | security |
| Vault | `vault` | Toggles vault state | security |
| Service switch | `service` | Toggles service state | security |

Each region has an `<inputtag>` and `<inputmask>` that connects it to the input port declared in [`mame-driver-plan.md`](mame-driver-plan.md).

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
