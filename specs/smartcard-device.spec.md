# Spec — Smart-card device

This spec defines the smart-card / memory-card device contract. See [`../docs/smartcard-emulation.md`](../docs/smartcard-emulation.md) for context.

## Purpose

Emulate ISO/IEC 7816 memory cards (and microprocessor cards on SKUs that support them).

## Firmware-facing interface

| Interface | Direction | Notes |
| --------- | --------- | ----- |
| Reset / clock control | W | Power, reset, clock to the card. |
| Data port | R/W | Serial line for ATR / APDU / memory protocol. |
| Status port | R | card_present, atr_ready, response_ready, error. |

## I/O ports

Per `fixtures/board/io-port-map.json` rows with `device = "smartcard"`.

## State machine

```
absent -> present_powered_off -> present_powered_on -> reset -> atr -> idle
idle  -> apdu_request -> apdu_response -> idle (T=0/T=1)
idle  -> memory_read|memory_write       -> idle (memory)
idle  -> remove -> absent
```

## Timing behavior

- `atr_delay_us` per fixture (ISO 7816-3).
- `apdu_response_delay_us` per fixture.

## Interrupts

Per board profile; default polled.

## MAME files

- `src/mame/coinline/millennium_smartcard.cpp/h`

## Fixture files

- `fixtures/cards/smartcard-valid.json`
- `fixtures/cards/smartcard-empty.json`

## Tests

- `tests/devices/test_smartcard_atr.cpp`
- `tests/devices/test_smartcard_apdu.cpp`
- `tests/devices/test_smartcard_memory_read.cpp`

## Boot milestone dependencies

- M5; smart-card scenarios depend on M10.

## Acceptance criteria

- ATR is honored; firmware proceeds.
- APDU exchange follows the spec for the selected protocol.
- Removing the card mid-operation produces the firmware's expected error path.
