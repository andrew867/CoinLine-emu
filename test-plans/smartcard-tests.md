# Test plan — Smart card

## Purpose

Verify smart-card / memory-card insert, ATR delivery, APDU exchange (when applicable), and remove.

## Prerequisites

- Built emulator.
- Firmware; board profile with `smartcard.present = true`.

## Fixtures

- `fixtures/cards/smartcard-valid.json`
- `fixtures/cards/smartcard-empty.json`

## Procedure

1. Run ATR unit tests for both fixtures.
2. Run APDU unit tests where the protocol is `t0` or `t1`.
3. Run memory-read tests where the protocol is `memory`.
4. Boot firmware to M10; insert each fixture in turn; observe firmware behavior.

## Expected behavior

- ATR delivered after `atr_delay_us`.
- APDU exchange follows ISO 7816-3 timing for the chosen protocol.
- Memory-card synchronous protocol matches per board profile.
- Removing the card mid-operation triggers firmware error.

## Pass criteria

- All unit tests pass.
- Firmware proceeds with the valid card; firmware reports an empty-card condition with the empty fixture.

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

- `tests/devices/test_smartcard_atr.cpp`
- `tests/devices/test_smartcard_apdu.cpp`
- `tests/devices/test_smartcard_memory_read.cpp`

## Cross-references

- [`../docs/smartcard-emulation.md`](../docs/smartcard-emulation.md), [`../specs/smartcard-device.spec.md`](../specs/smartcard-device.spec.md).
