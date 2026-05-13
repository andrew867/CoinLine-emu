# Spec — Card reader (magnetic)

This spec defines the magnetic card reader contract. See [`../docs/card-reader-emulation.md`](../docs/card-reader-emulation.md) for context.

## Purpose

Stream a magstripe payload with realistic timing and report success / LRC errors to the firmware.

## Firmware-facing interface

| Interface | Direction | Notes |
| --------- | --------- | ----- |
| Status port | R | card_present, head_contact, reading, read_error. |
| Data port | R | Streamed bits or bytes. |

## I/O ports

Per `fixtures/board/io-port-map.json` rows with `device = "card"`.

## State machine

```
idle -> card_inserted -> head_contact -> reading -> read_complete -> idle
                                              \-> read_error -> idle
```

## Timing behavior

- `swipe_duration_ms` per fixture.
- `bit_rate_bps` per fixture.

## Interrupts

Optional on `card_inserted` and `read_complete` per board profile.

## MAME files

- `src/mame/coinline/millennium_card.cpp/h`

## Fixture files

- `fixtures/cards/magcard-valid.json`
- `fixtures/cards/magcard-invalid-lrc.json`

## Tests

- `tests/devices/test_card_swipe_timing.cpp`
- `tests/devices/test_card_lrc_validation.cpp`
- `tests/devices/test_card_insert_remove.cpp`

## Boot milestone dependencies

- M5; card-call scenarios depend on M10.

## Acceptance criteria

- Valid swipe completes; firmware proceeds to a card call.
- Invalid LRC swipe is rejected with the firmware's expected error path.
