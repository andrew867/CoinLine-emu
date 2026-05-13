# Test plan — Modem UART

## Purpose

Verify the Z180 ASCI-driven modem UART, modem state machine, and DCD/CTS/RTS/DTR signal handling.

## Prerequisites

- Built emulator.
- Firmware; board profile.

## Fixtures

- `fixtures/modem/clean-connect.hex`
- `fixtures/modem/dropped-carrier.hex`
- `fixtures/modem/noisy-line.hex`
- `fixtures/scenarios/modem-connect.json`

## Procedure

1. Run UART register tests for ASCI 0 (and ASCI 1 if used).
2. Run modem state machine test for each fixture.
3. Run loopback test through the host bridge.
4. Run `modem-connect.json` end-to-end against a TCP loopback.

## Expected behavior

- ASCI registers respond per Z180 datasheet under firmware programming.
- Modem state machine progresses through all declared states.
- DCD / CTS / RTS / DTR honored per board profile.
- TX / RX bytes round-trip with bit-for-bit fidelity.
- Carrier loss injection triggers firmware recovery.
- Noisy-line injection produces CRC errors at the firmware layer.

## Pass criteria

- All ASCI and state machine tests pass.
- M8 reached.
- M11 reached on `modem-connect.json`.

## Fail criteria

- Bytes diverge between TX and RX in loopback.
- Carrier loss not handled.

## Evidence artifacts

- `uart-tx.hex`, `uart-rx.hex`.
- `host-bridge/transcript.jsonl`.

## Source files touched

- `src/mame/coinline/millennium_modem.cpp/h`
- `src/mame/coinline/millennium_hostbridge.cpp/h`

## Implementation files touched

- `tests/devices/test_modem_*.cpp`
- `tests/integration/test_host_bridge_*.cpp`

## Automated test location

- `tests/devices/test_modem_state_machine.cpp`
- `tests/devices/test_uart_transcript.cpp`
- `tests/integration/test_host_bridge_loopback.cpp`
- `tests/integration/test_host_bridge_carrier_loss.cpp`
- `tests/integration/test_host_bridge_noisy_line.cpp`
- `tests/integration/test_modem_connect.cpp`

## Cross-references

- [`../docs/modem-uart-host-bridge.md`](../docs/modem-uart-host-bridge.md).
- [`../specs/modem-uart-device.spec.md`](../specs/modem-uart-device.spec.md), [`../specs/host-bridge.spec.md`](../specs/host-bridge.spec.md).
