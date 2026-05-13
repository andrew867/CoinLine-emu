# Audio routing — MAME device implementation specification

## Overview

Implement **`millennium_audio_route_device`** together with **`millennium_telephony_device`**, because [`fixtures/board/audio-routing-state-map.json`](../fixtures/board/audio-routing-state-map.json) defines **telephony processor command bytes** (`0x20`–`0x46`) and effects — **not** bare Z180 port writes for most routing/mute actions.

Logical overview: [`docs/audio-routing-emulation.md`](../docs/audio-routing-emulation.md). Architecture: [`docs/audio-mame-device-architecture.md`](../docs/audio-mame-device-architecture.md).

## Classes and files

| Class | Header | Implementation |
| ----- | ------ | ---------------- |
| `millennium_audio_route_device` | `src/mame/coinline/millennium_audio_route.h` | `src/mame/coinline/millennium_audio_route.cpp` |
| `millennium_telephony_device` | `src/mame/coinline/millennium_telephony.h` | `src/mame/coinline/millennium_telephony.cpp` |

`millennium_telephony_device` owns decoding host→processor traffic (and processor→host status fan-out to supervision) per [`fixtures/board/audio-device-map.json`](../fixtures/board/audio-device-map.json) entry `telephony_processor_bridge`. Until `io-port-map.json` lists the exact bridge attachment points, treat wiring as **`compatibility_validation_required`** but still implement **decode stubs + traces** so firmware-driven bytes remain visible.

## Constructor / finder dependencies

- **Owner:** `millennium_state`
- **Required finders:** references or delegates to `millennium_modem_device`, `millennium_hostbridge_device` (whichever carries bytes today), `millennium_keypad_device` / hookswitch input
- **Notifications:** `void notify_voice_active(bool)` from `millennium_voiceware_device`; `void notify_hook(bool off_hook)`; `void notify_carrier(bool up)` from modem path

## Route / call state machine (normative)

Orthogonal state variables (all traced on change):

| Variable | Meaning |
| -------- | ------- |
| `call_state` | One of `CALL_IDLE`, `CALL_DIALING`, `CALL_ESTAB_UNSUPVSED`, `CALL_ESTAB_INCOMING`, `CALL_ESTAB_SUPVSED`, `CALL_ESTAB_UNSVD_CHARGE`, `CALL_ESTAB_SVD_CHARGE` — driven **only** by decoding `CALL_STATE_*` command bytes (`0x40`–`0x46`) from the routing map |
| `tx_muted`, `rx_muted` | From `TX_MUTE`/`TX_UNMUTE` (`0x26`/`0x27`) and `RX_MUTE`/`RX_UNMUTE` (`0x28`/`0x29`) |
| `sidetone_suppressed` | From `SIDETONE_SUPPRESSION_ON`/`OFF` (`0x2A`/`0x2B`) |
| `voice_prompt_conditioning` | Sequences `voice_prompt_conditioning_connected` in routing map — apply **only** when voiceware asserts prompt path + profile marks CO connected |

**Mute state machine** is the tuple `(tx_muted, rx_muted, sidetone_suppressed)` with transitions **only** from decoded command bytes + profile defaults at `device_reset`.

## Port / binding placeholders

| Source | Binding |
| ------ | ------- |
| Telephony commands | Hex keys under `commands` in [`fixtures/board/audio-routing-state-map.json`](../fixtures/board/audio-routing-state-map.json) |
| Direct Z180 I/O | Additional PIO bits — **only** if promoted into [`fixtures/board/io-port-map.json`](../fixtures/board/io-port-map.json); until then **do not invent ports** |

## Interaction contracts

| Partner | Contract |
| ------- | -------- |
| `millennium_voiceware_device` | Call `notify_voice_active(true)` from `VW_PLAYING`; false on idle/fault |
| Hookswitch | Off-hook forces handset-capable routes per profile — encode as compatibility row if profile unspecified |
| Modem | Carrier loss clears established-call substates unless supervision overrides |
| `millennium_supervision_device` | Receives classified disconnect hints after status-code fusion (see supervision spec) |

## Failure / default states

Power-on composite state **must** match [`fixtures/audio/audio-route-idle.json`](../fixtures/audio/audio-route-idle.json). Divergence → [`docs/compatibility-validation-items.md`](../docs/compatibility-validation-items.md), not silent hacks.

## Trace events (minimum)

| `event_type` | When |
| -------------- | ---- |
| `route_change` | Any change to `call_state` or route mux snapshot |
| `mute_change` | TX/RX/sidetone mute tuple changes |
| `telephony_command_decode` | Each recognized command byte from bridge with hex + effect enum |
| `route_conflict_resolved` | Voiceware vs line priority resolved per profile |

## Tests

[`test-plans/audio-routing-mame-implementation-tests.md`](../test-plans/audio-routing-mame-implementation-tests.md).

## Acceptance criteria

- [ ] Fixture replay (Class A) reproduces golden `route_change` / `mute_change` / `telephony_command_decode` sequences **without** firmware.
- [ ] **Class C** (firmware) correlates hookswitch / modem activity with traces — **requires** `coinline-mame.exe` run per [`docs/audio-ci-plan.md`](../docs/audio-ci-plan.md).
- [ ] No scenario JSON field sets `route_state` or mute tuple directly.
