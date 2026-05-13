# Test plan — Coin validator

## Purpose

Verify pulse-train decoding for every declared denomination, jam reporting, and disable behavior.

## Prerequisites

- Built emulator.
- Firmware; board profile with `coin.validator_type != "none"`.

## Fixtures

- `fixtures/board/board-profile-2line-vfd.json` (denominations field).
- `fixtures/scenarios/coin-call.json`.

## Procedure

1. Run pulse-train unit test for every denomination.
2. Run jam injection test.
3. Run disable test (firmware-issued disable, then attempt insertion).
4. Run `coin-call.json` end-to-end.

## Expected behavior

- Each denomination produces the firmware's expected accept event.
- Jam triggers firmware error path.
- Disabled validator does not accept coins.

## Pass criteria

- All unit tests pass.
- `coin-call.json` reaches M11.

## Fail criteria

- Denomination misidentified.
- Jam not reported.

## Evidence artifacts

- `io-trace.jsonl`.
- `vfd/snapshots/*.json`.

## Source files touched

- `src/mame/coinline/millennium_coin.cpp/h`

## Implementation files touched

- `tests/devices/test_coin_*.cpp`

## Automated test location

- `tests/devices/test_coin_pulse_train.cpp`
- `tests/devices/test_coin_jam.cpp`
- `tests/devices/test_coin_disable.cpp`
- `tests/integration/test_coin_call.cpp`

## Cross-references

- [`../docs/coin-validator-emulation.md`](../docs/coin-validator-emulation.md), [`../specs/coin-validator-device.spec.md`](../specs/coin-validator-device.spec.md).
