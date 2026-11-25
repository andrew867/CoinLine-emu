# Test plan — Smart card

## Purpose

Verify smart-card / memory-card insert, ATR delivery, APDU exchange (when applicable), and remove.

## Prerequisites

- Built emulator.
- Firmware; board profile with `smartcard.present = true`.

## Fixtures

- `fixtures/cards/smartcard-sync-test.json`
- `fixtures/cards/smartcard-async-t0-lab.json`
- `fixtures/cards/smartcard-async-t1-lab.json`

## Procedure

1. Run synchronous contact, memory-persistence and asynchronous protocol tests.
2. Run native synthetic later-board contact probes and the selected ASCI0,
   including direct/inverse T=0, T=1 and bounded PPS. Verify the earlier
   mapping with an actual firmware runtime contact trace.
3. Boot a compatible installed firmware and exercise its actual Craft
   contact check with the synchronous fixture, then remove it and return
   to normal service.
4. In normal-call mode, present the unfunded fixture without dialing and
   verify the firmware's rejection/removal behavior without a payment.
5. Qualify historical applications separately with authorized lab references.

## Expected behavior

- ATR delivered after `atr_delay_us`.
- APDU exchange follows ISO 7816-3 timing for the chosen protocol.
- Memory-card synchronous protocol matches per board profile.
- Removing the card mid-operation triggers firmware error.

## Pass criteria

- All unit tests pass.
- Generic contacts return the synthetic test data, and the applicable
  firmware self-test passes. A normal-call rejection of the non-payment
  fixture is expected, not an application-acceptance failure.

## Fail criteria

- ATR not honored.
- Wrong APDU response timing.

## Evidence artifacts

- `io-trace.jsonl`.
- `vfd/snapshots/*.json`.

## Source files touched

- `src/mame/coinline/millennium_smartcard.cpp/h`

## Implementation files touched

- `tests/devices/test_smartcard_*.cpp`

## Automated test location

- `tests/devices/test_smartcard_protocol_negotiation.cpp`
- `tests/devices/test_smartcard_async_transport.cpp`
- `tools/acceptance/smartcard_pin_probe.py`
- `tools/acceptance/smartcard_async_probe.py`
- `tools/acceptance/smartcard_craft_probe.py`

## Cross-references

- [`../docs/smartcard-emulation.md`](../docs/smartcard-emulation.md), [`../specs/smartcard-device.spec.md`](../specs/smartcard-device.spec.md).
