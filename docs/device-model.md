# Device model

This document defines the common contract every device in `coinline-emu` must implement. It is the cross-cutting reference that per-device docs (`vfd-emulation.md`, `keypad-emulation.md`, `modem-uart-host-bridge.md`, [`voiceware-emulation.md`](voiceware-emulation.md), [`audio-routing-emulation.md`](audio-routing-emulation.md), [`disconnect-supervision-emulation.md`](disconnect-supervision-emulation.md), etc.) build on.

**Audio subsystem (MAME track):** Logical devices map to `millennium_voiceware_device`, **`millennium_telephony_device`** (command/status bridge per [`fixtures/board/audio-device-map.json`](../fixtures/board/audio-device-map.json)), `millennium_audio_route_device`, `millennium_supervision_device`, and extensions to `millennium_audio_device` per [`audio-mame-device-architecture.md`](audio-mame-device-architecture.md). Structured trace lines follow [`specs/audio-trace-format.spec.md`](../specs/audio-trace-format.spec.md) (JSON Lines exports `audio-trace.jsonl`, `voiceware-trace.jsonl`, `supervision-trace.jsonl`, `alerter-trace.jsonl`).

## Concepts

- A **device** is a unit of hardware emulation owning one or more I/O ports, optionally a memory region, and possibly one or more interrupt sources.
- A device is **deterministic** given its inputs and the cycle counter.
- A device is **observable** via traces and snapshots.
- A device is **scriptable** via fixtures and scenarios.

## Required interface

Every device implements the following operations. The MAME track inherits them from MAME's `device_t` and `device_execute_interface`. The MIT-clean track implements an equivalent base class.

| Operation | When called | Notes |
| --------- | ----------- | ----- |
| `reset()` | At machine reset | Restores documented reset values; clears any latched events. |
| `read(port, &value)` | On firmware I/O read | Returns the device's response; updates internal state if the port is read-side-effecting. |
| `write(port, value)` | On firmware I/O write | Updates internal state; possibly raises an interrupt. |
| `tick(cycles)` | On scheduler tick | Advances any internal timers; emits interrupts as appropriate. |
| `interrupt_pending()` | By the interrupt controller | Returns whether the device has a pending interrupt and its vector. |
| `snapshot()` | On save state | Serializes internal state to a portable representation. |
| `restore(snapshot)` | On load state | Restores from `snapshot()`. |
| `trace_emit(level, event)` | Internally | Emits a structured trace event. |
| `inject(fixture)` | By scenarios | Pre-loads a fixture (e.g., a magstripe payload). |

## Reset values

Every device's `reset()` must produce values that exactly match the device's spec under [`../specs/`](../specs/) (e.g., `vfd-device.spec.md`'s `reset_values` section). Tests in `tests/devices/test_<device>_reset.cpp` enforce this.

## I/O semantics

- **Reads** never have undocumented side effects. If a read is side-effecting, the spec says so explicitly.
- **Writes** are accepted as documented. Unsupported bit patterns are logged with the device's trace tag.
- Out-of-range writes (e.g., writing to a read-only port) are logged but not silently dropped.

## Tick semantics

- Every device has a clock or cycle reference that drives any internal timers.
- The reference cycle source is the Z180's CPU cycle counter unless the device spec specifies otherwise (e.g., the modem UART uses the ASCI's bit-clock-derived reference).
- A device's `tick(cycles)` must be safe to call with arbitrary cycle deltas; missed ticks must be telescoped, not dropped.

## Interrupt generation

- Devices request interrupts through the interrupt controller per [`interrupt-map.md`](interrupt-map.md).
- A device must clear its interrupt request only when the firmware acknowledges it (per the device's spec) — never on a free schedule.

## Snapshot / restore

- Snapshots are portable across runs of the same emulator binary.
- Restoring a snapshot must produce a state byte-equivalent to the original.
- Snapshot format is JSON-based for the MIT-clean track and uses MAME's binary state format for the MAME track. Both tracks expose a JSON export for evidence bundles.

## Trace logging

A trace event has shape:

```jsonc
{
  "device": "vfd",
  "ts": "2026-05-03T11:00:00.012345Z",
  "cycle": 1234567890,
  "event": "command",
  "level": "info",
  "data": { "raw": "0x1B 0x4F", "decoded": "clear screen" }
}
```

Levels: `error`, `warn`, `info`, `debug`, `trace`. The default level is `info`. CI runs at `debug` for failing tests so reproductions are easier.

## Fixture injection

A scenario may inject a fixture into a device:

- Magnetic card swipe: `inject({ "type": "magstripe", "payload": "..." })`.
- Smart card insert: `inject({ "type": "smartcard", "atr": "..." })`.
- Coin pulse train: `inject({ "type": "coin", "denomination": 25, "pulses": [...] })`.
- Modem byte stream: `inject({ "type": "modem_rx_bytes", "hex": "..." })`.
- Telephony supervision stream: `inject({ "type": "telephony_status", "codes": ["0x64", "0x66"] })`.
- Voice playback completion: `inject({ "type": "voice_complete", "delay_ms": 1200 })`.
- Lock / door / vault state: `inject({ "type": "security", "lock": false })`.

Fixtures are validated against per-device schemas before injection; invalid fixtures fail the scenario.

## Determinism

A device is deterministic if, given the same inputs (resets, I/O writes, fixtures injected, ticks consumed) and the same cycle counter, it produces the same outputs. Determinism is verified by:

- `tests/devices/test_<device>_*.cpp` — runs the same input twice and compares outputs.
- Saved-state replay in regression tests.

## MAME integration approach

The MAME track represents each device as a `device_t` subclass with `device_execute_interface` (for ticking) and, where appropriate, `device_image_interface` (for slot devices like card readers). The driver wires devices into the machine via `MCFG_DEVICE_ADD` (or modern `device.set_xxx()` form) per [`mame-driver-plan.md`](mame-driver-plan.md).

## Per-device docs

| Device | Doc | Spec |
| ------ | --- | ---- |
| VFD | [`vfd-emulation.md`](vfd-emulation.md) | [`../specs/vfd-device.spec.md`](../specs/vfd-device.spec.md) |
| Keypad | [`keypad-emulation.md`](keypad-emulation.md) | [`../specs/keypad-device.spec.md`](../specs/keypad-device.spec.md) |
| Hookswitch + handset | [`hookswitch-and-handset.md`](hookswitch-and-handset.md) | within keypad / audio specs |
| Card reader | [`card-reader-emulation.md`](card-reader-emulation.md) | [`../specs/card-reader-device.spec.md`](../specs/card-reader-device.spec.md) |
| Smart card | [`smartcard-emulation.md`](smartcard-emulation.md) | [`../specs/smartcard-device.spec.md`](../specs/smartcard-device.spec.md) |
| Coin validator | [`coin-validator-emulation.md`](coin-validator-emulation.md) | [`../specs/coin-validator-device.spec.md`](../specs/coin-validator-device.spec.md) |
| Modem UART | [`modem-uart-host-bridge.md`](modem-uart-host-bridge.md) | [`../specs/modem-uart-device.spec.md`](../specs/modem-uart-device.spec.md) |
| NVRAM + tables | [`nvram-and-table-storage.md`](nvram-and-table-storage.md) | [`../specs/nvram-storage-device.spec.md`](../specs/nvram-storage-device.spec.md) |
| Alerter audio | [`alerter-audio.md`](alerter-audio.md) | [`../specs/alerter-audio-device.spec.md`](../specs/alerter-audio-device.spec.md) |
| Lock / door / vault / service | [`lock-door-vault-service.md`](lock-door-vault-service.md) | [`../specs/lock-door-service-device.spec.md`](../specs/lock-door-service-device.spec.md) |

## Cross-references

- [`io-port-map.md`](io-port-map.md), [`memory-map.md`](memory-map.md), [`interrupt-map.md`](interrupt-map.md) — what devices claim.
- [`scenario-runner.md`](scenario-runner.md) — how fixtures get injected.
- [`evidence-bundles.md`](evidence-bundles.md) — what traces are exported.
