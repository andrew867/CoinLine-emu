# Test plan — Card reader

## Purpose

Verify magstripe streaming, LRC validation, and insert / remove cycles.

## Prerequisites

- Built emulator.
- Firmware; board profile with `card_reader.present = true`.

## Fixtures

- `fixtures/cards/magcard-valid.json`
- `fixtures/cards/magcard-invalid-lrc.json`
- `fixtures/scenarios/card-call.json`

## Procedure

1. Run timing unit test against synthetic byte stream.
2. Run LRC validation unit test on both valid and invalid fixtures.
3. Boot firmware to M10.
4. Run `card-call.json` swiping the valid card; verify firmware proceeds to a card call.
5. Run a variant scenario swiping the invalid LRC card; verify firmware rejects and shows error.

## Expected behavior

- Bit timing matches `bit_rate_bps` from the fixture.
- Valid LRC accepted.
- Invalid LRC rejected with error path triggered.
- Insert / remove during read produces firmware error.

## Pass criteria

- All unit tests pass.
- Valid swipe leads to host call (M11).
- Invalid swipe leads to firmware error VFD message.

## Fail criteria

- Timing mismatch.
- Valid swipe rejected.
- Invalid swipe accepted.

## Evidence artifacts

- `io-trace.jsonl`.
- `vfd/snapshots/*.json`.
- `host-bridge/transcript.jsonl` (for the valid path).

## Source files touched

- `src/mame/coinline/millennium_card.cpp/h`

## Implementation files touched

- `tests/devices/test_card_*.cpp`

## Automated test location

- `tests/devices/test_card_swipe_timing.cpp`
- `tests/devices/test_card_lrc_validation.cpp`
- `tests/devices/test_card_insert_remove.cpp`
- `tests/integration/test_card_call.cpp`

## Cross-references

- [`../docs/card-reader-emulation.md`](../docs/card-reader-emulation.md), [`../specs/card-reader-device.spec.md`](../specs/card-reader-device.spec.md).
