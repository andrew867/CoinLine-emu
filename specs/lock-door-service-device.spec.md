# Spec — Lock / door / vault / service device

This spec defines the security-input device contract. See [`../docs/lock-door-vault-service.md`](../docs/lock-door-vault-service.md) for context.

## Purpose

Reflect physical security and service states to the firmware.

## Firmware-facing interface

| Input | Direction | Notes |
| ----- | --------- | ----- |
| Lock | R | Active level per board profile. |
| Door | R | Active level per board profile. |
| Vault | R | Active level per board profile. |
| Service switch | R | Momentary or latching per board profile. |

## I/O ports

Per `fixtures/board/io-port-map.json` rows with `device = "security"`.

## State machine

Each input is a binary line.

## Timing behavior

- `debounce_cycles` per input.

## Interrupts

Service switch may map to `/INT2` per board profile.

## MAME files

- `src/mame/coinline/millennium_security.cpp/h`

## Fixture files

None standalone; driven by scenario verbs `set_lock_state`, `set_door_state`, `set_vault_state`, `set_service_state`.

## Tests

- `tests/devices/test_security_inputs.cpp`
- `tests/devices/test_security_service_debounce.cpp`
- `tests/integration/test_service_mode_entry.cpp` (parses `fixtures/scenarios/service-mode.json` and writes a spec-shaped evidence bundle)

## Boot milestone dependencies

- M5; service mode scenarios depend on M10.

## Acceptance criteria

- Each input changes firmware behavior in the expected way.
- Service-mode entry is reproducible under `service-mode.json`.
