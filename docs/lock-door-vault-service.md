# Lock, door, vault, and service-mode inputs

This document describes the security-related discrete inputs: lock state, door open/closed, vault open/closed, and service-mode switch.

## Purpose

Reflect physical security and service states to the firmware so it can:

- Refuse certain operations when the lock is engaged.
- Record door / vault open events for audit.
- Enter service mode when the service switch is asserted.

## Firmware-facing interface

| Interface | Description |
| --------- | ----------- |
| Lock | Discrete input; active level per board profile. |
| Door | Discrete input. |
| Vault | Discrete input. |
| Service | Discrete input; per profile may be momentary or latching. |

## State machine

Each input is a binary line; combined state is interpreted by firmware.

## Timing

Debounce: configurable per input.

## Interrupts

The service switch may raise `/INT2` (per profile). Lock / door / vault are typically polled.

## Tests

- `tests/devices/test_security_inputs.cpp` — verifies each input round-trips.
- `tests/devices/test_security_service_debounce.cpp` — service switch line debounces before asserting.
- `tests/integration/test_service_mode_entry.cpp` — validates `service-mode.json` and evidence bundle layout.

## Boot-milestone dependencies

- M5: first security read observed.
- Service-mode scenarios depend on M10.

## Acceptance criteria

- Each input changes firmware behavior in the expected way (lock prevents operator commands, vault open is logged, service switch enters service mode).

## Cross-references

- [`../specs/lock-door-service-device.spec.md`](../specs/lock-door-service-device.spec.md).
- [`../test-plans/regression-tests.md`](../test-plans/regression-tests.md).
