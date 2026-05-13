# Test plan — I/O ports

## Purpose

Verify the I/O port map decoder routes known ports to their owning devices and logs unknown ports per the unknown-port policy.

## Prerequisites

- Built emulator.
- Firmware; board profile.
- `fixtures/board/io-port-map.json` validated against [`../specs/io-port-map.spec.md`](../specs/io-port-map.spec.md).

## Fixtures

- `fixtures/board/io-port-map.json`
- `fixtures/board/board-profile-2line-vfd.json`

## Procedure

1. Schema validation: `pytest tests/fixtures/test_io_port_map_schema.py`.
2. Per-port default tests for every known port.
3. Unknown-port logging test.
4. Boot firmware and observe at least one read/write per known port appears in `io-trace.jsonl`.

## Expected behavior

- Every known port routes to its owning device's handler.
- Reading an unknown port returns the configured `unknown_default` and produces a structured log entry with PC, cycle, port, value, and direction.
- Writing an unknown port discards the value and produces a structured log entry.
- `on_unknown = "log_and_halt"` halts the CPU after one entry; default `"log_and_continue"` does not halt.

## Pass criteria

- All schema, default, and unknown-port-logging tests pass.
- For each known port, at least one matching trace entry appears during boot (subject to firmware reaching M5+).

## Fail criteria

- An unknown port produces no log entry.
- A known port falls through to the unknown logger.
- `on_unknown = "log_and_halt"` does not halt the CPU.

## Evidence artifacts

- `io-trace.jsonl`.
- Unknown-port log entries.

## Source files touched

- `src/mame/coinline/millennium_io.cpp`

## Implementation files touched

- `tests/fixtures/test_io_port_map_schema.py`
- `tests/devices/test_unknown_port_logging.cpp`
- `tests/devices/test_io_port_defaults.cpp`

## Automated test location

- `tests/fixtures/test_io_port_map_schema.py`
- `tests/devices/test_unknown_port_logging.cpp`
- `tests/devices/test_io_port_defaults.cpp`
- `tests/devices/test_io_port_*` per device

## Cross-references

- [`../docs/io-port-map.md`](../docs/io-port-map.md).

