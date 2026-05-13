# Test plan — Artwork layout

## Purpose

Verify the MAME `.lay` artwork: clickable regions are aligned with the front-panel image, all front-panel inputs are accessible, and screenshot evidence matches references.

## Prerequisites

- Built emulator.
- Front-panel image at `artwork/millennium-terminal-front.png`.
- `artwork/millennium.lay` present.

## Fixtures

- `artwork/millennium-terminal-front.png` (operator-supplied from `docs/images/terminal-unbranded.webp`).
- `artwork/millennium.lay`.

## Procedure

1. Run a coordinate sanity test: every region in the `.lay` lies within image bounds.
2. Run a screenshot diff against reference images for boot-to-idle and a card-swipe state.
3. Confirm every clickable region has an `inputtag` / `inputmask` connecting it to the right input port.

## Expected behavior

- Region coordinates within the image bounds.
- Inputs round-trip when clicked through the artwork.
- VFD overlays render at the expected screen location.

## Pass criteria

- Coordinate test passes.
- Screenshot diff within tolerance.
- Every input round-trips.

## Fail criteria

- A region overflows the image.
- An input does not produce the expected matrix or discrete signal when clicked.
- Screenshot diff exceeds tolerance.

## Evidence artifacts

- `screenshots/*.png` per scenario step.

## Source files touched

- `artwork/millennium.lay`

## Implementation files touched

- `tests/integration/test_artwork_*.cpp`

## Automated test location

- `tests/integration/test_artwork_regions.cpp`
- `tests/integration/test_artwork_idle.cpp`
- `tests/integration/test_artwork_card_swipe.cpp`
- `tests/integration/test_artwork_service_mode.cpp`

## Cross-references

- [`../docs/artwork-and-layout.md`](../docs/artwork-and-layout.md).
