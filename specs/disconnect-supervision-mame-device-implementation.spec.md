# Disconnect supervision — MAME device implementation specification

## Overview

Implement **`millennium_supervision_device`** consuming **processor→host telephony status codes** from [`fixtures/board/disconnect-supervision-map.json`](../fixtures/board/disconnect-supervision-map.json) (`processor_to_host_codes`), fused with modem/carrier timing. Logical behavior: [`docs/disconnect-supervision-emulation.md`](../docs/disconnect-supervision-emulation.md).

## Class and files

| Item | Value |
| ---- | ----- |
| Class | `millennium_supervision_device` |
| Headers | `src/mame/coinline/millennium_supervision.h` |
| Implementation | `src/mame/coinline/millennium_supervision.cpp` |

## Input bindings (normative)

| Input | Hex examples | Source |
| ----- | -------------- | ------ |
| Status codes | `0x60`, `0x62`, `0x64`, `0x66`, `0x68`, `0x6A`, `0x6C`, `0x6E`, `0x8C`, `0x8E` | Decoded frames from `millennium_telephony_device` / host-bridge RX path — **not** invented CPU ports |
| Timer `cutoff_on_disconnect_duration` | ROM/profile tick count | Map `timers.cutoff_on_disconnect_duration` — convert to emulated cycles only after tick base validated (**`compatibility_validation_required`** until then) |

Dedicated Z180 supervision ports, if any, appear only after **`io-port-map.json`** lists them.

## Auxiliary / line model

Finite-state abstraction **only**:

- **Polarity / reversal class:** treat `LINE_REVERSAL_0/1` codes as inputs to a reversal sub-state (`REV_IDLE`, `REV_SEEN`) — timing thresholds from profile, not guessed milliseconds.
- **CPC vs normal disconnect:** emit distinct `disconnect_event` enums **only** when profile + traces distinguish them; else emit generic `disconnect_pending` + `compatibility_validation_required`.

## Disconnect FSM (normative)

| State | Meaning |
| ----- | ------- |
| `SUP_IDLE` | No active teardown |
| `SUP_CODES_STREAMING` | Receiving valid status codes |
| `SUP_DISCONNECT_PENDING` | Condition latched (carrier dropped + code pattern, etc.) |
| `SUP_DISCONNECT_EMITTED` | `disconnect_event` trace emitted |
| `SUP_CLEARED` | Firmware-visible acknowledge / new call arms |

Parallel **watchdog:** `TW_OFF` | `TW_RUN` | `TW_FIRED` for map timers.

## Trace events (minimum)

| `event_type` | When |
| -------------- | ---- |
| `supervision_status_code` | Each `processor_to_host` code received with raw hex |
| `supervision_status_read` | Firmware read of supervision aggregate register — **when** such a register exists in map |
| `disconnect_event` | Typed emission `normal_disconnect` \| `cpc` \| `timeout` \| `fault` per profile |
| `supervision_timeout` | Watchdog fired |

## Tests

[`test-plans/disconnect-supervision-mame-implementation-tests.md`](../test-plans/disconnect-supervision-mame-implementation-tests.md).

## Acceptance criteria

- [ ] Class A fixture replay emits deterministic `supervision_status_code` + `disconnect_event` sequences without firmware.
- [ ] Class C requires **`coinline-mame.exe`** + firmware; asserts traces from that run only.
- [ ] Scenarios may inject **modem/bridge bytes** at transport boundary — never inject `disconnect_event` JSON into emulator state.
