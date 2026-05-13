# Card reader emulation

This document describes the magnetic-stripe card reader (magstripe). Smart-card behavior is documented in [`smartcard-emulation.md`](smartcard-emulation.md).

## Purpose

Stream a magnetic-stripe payload to the firmware with realistic timing, including LRC validation, head-contact detection, and read errors.

## Firmware-facing interface

| Interface | Description |
| --------- | ----------- |
| Status port | Card-present, head-contact, read-in-progress, error flags. |
| Data port | Streamed bits (or bytes, per board profile). |

Port numbers are pinned in the active board profile.

## State machine

```
idle -> card_inserted -> head_contact -> reading -> read_complete -> idle
                                              \-> read_error -> idle
```

## Timing

- Swipe duration: configurable per fixture (`swipe_duration_ms` typically 100–800 ms).
- Bit rate: typical 75 bps (track 1) or 210 bps (track 2); per fixture.
- Inter-character gap: per fixture.

## Interrupts

The reader may raise an interrupt on `card_inserted` and `read_complete`; per board profile.

## Fixtures

- `fixtures/cards/magcard-valid.json` — valid track 2 swipe with correct LRC.
- `fixtures/cards/magcard-invalid-lrc.json` — same payload with intentionally wrong LRC for failure-mode tests.

Fixture format:

```jsonc
{
  "type": "magstripe",
  "track": 2,
  "payload": "...",
  "swipe_duration_ms": 350,
  "bit_rate_bps": 210,
  "force_lrc_error": false
}
```

## Tests

- `tests/devices/test_card_swipe_timing.cpp` — verifies bit timing.
- `tests/devices/test_card_lrc_validation.cpp` — verifies LRC behavior for valid and invalid payloads.
- `tests/devices/test_card_insert_remove.cpp` — verifies insert / remove cycle.

## Boot-milestone dependencies

- M5: first card-status read observed (card device addressed).
- The card-call scenario depends on M10 (idle) for entry.

## Acceptance criteria

- Valid swipe completes; firmware proceeds to a card call.
- Invalid LRC swipe is rejected with the firmware's expected error path.
- Insert / remove during read produces the firmware's expected error message.

## Cross-references

- [`../specs/card-reader-device.spec.md`](../specs/card-reader-device.spec.md).
- [`../test-plans/card-reader-tests.md`](../test-plans/card-reader-tests.md).
- [`smartcard-emulation.md`](smartcard-emulation.md).
