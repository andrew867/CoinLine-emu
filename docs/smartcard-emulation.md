# Smart-card emulation

This document describes the smart-card / memory-card device. The Millennium-compatible terminal supports ISO/IEC 7816 memory cards (and, on some SKUs, microprocessor cards). The exact protocol stack is per board profile.

## Purpose

Provide an emulated smart card that responds to firmware-issued reset/ATR/APDU sequences (microprocessor) or memory-card synchronous protocols (memory).

## Firmware-facing interface

| Interface | Description |
| --------- | ----------- |
| Reset / clock control | Discrete signals controlling card power, reset, and clock. |
| Data port | Serial line for ATR / APDU / memory-card responses. |
| Status port | Card-present, ATR-received, response-ready, error flags. |

## State machine

```
absent -> present_powered_off -> present_powered_on -> reset -> atr -> idle
idle  -> apdu_request -> apdu_response -> idle
idle  -> remove -> absent
```

For memory cards, the `apdu_*` states are replaced by synchronous read / write states per the memory protocol (e.g., I²C-style 3-wire).

## Timing

- ATR delay: per ISO 7816-3 timing (varies with card; per fixture).
- APDU response delay: per fixture.
- Reset duration: per ISO 7816-3.

## Interrupts

Per board profile. Default behavior is polled.

## Fixtures

- `fixtures/cards/smartcard-valid.json` — known-valid card with ATR + memory contents.
- `fixtures/cards/smartcard-empty.json` — empty / blank card with default ATR.

Fixture format:

```jsonc
{
  "type": "smartcard",
  "protocol": "memory" | "t0" | "t1",
  "atr": "0x3B 0x...",
  "memory": "...",
  "apdu_responses": [...],
  "atr_delay_us": 4000
}
```

## Tests

- `tests/devices/test_smartcard_atr.cpp` — verifies ATR delivery.
- `tests/devices/test_smartcard_apdu.cpp` — verifies APDU exchange (when applicable).
- `tests/devices/test_smartcard_memory_read.cpp` — verifies memory-card read.

## Boot-milestone dependencies

- M5: first smart-card I/O observed.
- Smart-card scenarios depend on M10 (idle).

## Acceptance criteria

- ATR is honored; firmware proceeds to the next protocol state.
- APDU exchange follows the spec for the selected protocol.
- Removing the card triggers the firmware's expected error path.

## Cross-references

- [`../specs/smartcard-device.spec.md`](../specs/smartcard-device.spec.md).
- [`../test-plans/smartcard-tests.md`](../test-plans/smartcard-tests.md).
- [`card-reader-emulation.md`](card-reader-emulation.md).
