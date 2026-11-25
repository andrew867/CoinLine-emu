# Spec — Smart-card device

This spec defines the smart-card / memory-card device contract. See [`../docs/smartcard-emulation.md`](../docs/smartcard-emulation.md) for context.

## Purpose

Emulate ISO/IEC 7816 memory cards (and microprocessor cards on SKUs that support them).

## Firmware-facing interface

| Interface | Direction | Notes |
| --------- | --------- | ----- |
| Reset / clock control | W | Power, reset, clock to the card. |
| Data port | R/W | Serial line for ATR / APDU / memory protocol. |
| Status inputs | R | Card presence and synchronous contact data; asynchronous status is the native ASCI0 register state. |

## I/O ports

The earlier board uses a latched pulse on PIO A bit 6 and active-low clock
and reset outputs on PIO C bits 6 and 7. The later board uses the manual
D/E/F contact selection and supply latch; customer asynchronous operation
uses the selected ASCI0 route. See the hardware mapping in the device guide.

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
- `src/mame/coinline/millennium_smartcard_model.cpp/h`

## Fixture files

- `fixtures/cards/smartcard-sync-test.json`
- `fixtures/cards/smartcard-async-t0-lab.json`
- `fixtures/cards/smartcard-async-t1-lab.json`

## Tests

- `tests/devices/test_smartcard_protocol_negotiation.cpp`
- `tests/devices/test_smartcard_async_transport.cpp`
- `tools/acceptance/smartcard_pin_probe.py`
- `tools/acceptance/smartcard_async_probe.py`
- `tools/acceptance/smartcard_craft_probe.py`

## Boot milestone dependencies

- M5; smart-card scenarios depend on M10.

## Acceptance criteria

- The synchronous fixture returns its own contact bits on the actual board
  pins, and the selected real firmware passes its contact check where offered.
- The generic asynchronous fixture returns its ATR and read-only APDU
  responses through the native selected UART; unsupported operations fail.
- Removal clears the active contact session and pending serial traffic.
- Historical application recognition and settlement require independent
  authorized lab acceptance and are not implied by the preceding gates.
