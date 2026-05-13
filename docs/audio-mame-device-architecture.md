# Audio / supervision — MAME device architecture

This document describes how voice playback, audio routing, disconnect supervision, and alerter output integrate into the **`cl_millennium`** machine driver in `coinline-emu`. Behavior is **firmware-driven**: the CPU performs real I/O reads/writes; devices react and emit traces. No application-layer simulation of call audio.

## Conformance and forbidden shortcuts

| Allowed conformance evidence | Disallowed for proving **firmware-driven** audio/supervision |
| ---------------------------- | ----------------------------------------------------------- |
| **`coinline-mame.exe`** / **`mamecoinline.exe`** running the **real** firmware image, plus JSONL traces from that process (`build/runs/`, evidence bundle) | Any **browser** harness (Web Audio API, WASM UI, headless Chromium) asserting audio equivalence |
| **GoogleTest** calling `millennium_*_device::read`/`write` **without** firmware — only when the test is explicitly **Class A** (*device fixture*, not firmware proof) | Labeling those tests as firmware verification |
| Scenario runner toggling **physical inputs** (hook, keypad, modem RX inject at UART/bridge boundary) | Scenario JSON that sets internal voiceware/route/supervision state or “prompt playing” expectations directly |

**Firmware-proof rule:** If a test claims the **firmware** exercised voiceware/routing/supervision, it **must** have launched the MAME-track emulator binary with firmware loaded. Parsing traces from any other process is not sufficient.

## Subsystem placement

| Logical role | MAME implementation target | Notes |
| ------------ | -------------------------- | ----- |
| Voice prompts / phrase control | `millennium_voiceware_device` | **Z180 I/O:** `0x40` / `0x42` / `0x61` per [`fixtures/board/voiceware-command-map.json`](../fixtures/board/voiceware-command-map.json) |
| Telephony command/status bridge | `millennium_telephony_device` | Decodes bytes per [`fixtures/board/audio-routing-state-map.json`](../fixtures/board/audio-routing-state-map.json); sits on modem/host path per [`fixtures/board/audio-device-map.json`](../fixtures/board/audio-device-map.json) |
| Route / mute matrix | `millennium_audio_route_device` | Driven by telephony commands + hookswitch + modem + `notify_voice_active` |
| Local tones / buzzer | `millennium_audio_device` **extended** | PIO/control bits **OPEN** until pinned in `audio-device-map.json` — traces remain mandatory |
| Line disconnect / supervision | `millennium_supervision_device` | Processor→host status codes per [`fixtures/board/disconnect-supervision-map.json`](../fixtures/board/disconnect-supervision-map.json) |
| Cross-cutting traces | `millennium_audio_trace` (`millennium_audio_trace.cpp`) | Shared JSONL emission |

Devices are **`device_t` children** of `millennium_state`. I/O enters via `millennium_io.cpp` and/or **`millennium_telephony_device`** callbacks from `millennium_modem_device` / host bridge per [`io-port-map.md`](io-port-map.md) and board fixtures.

## Data flow (ASCII)

```
Firmware I/O reads/writes
        |
        v
+-------------------+
| millennium_io map |
+----+--------------+
     |
     +--> millennium_voiceware_device (ports 0x40/0x42/0x61)
     |         |
     |         +--> notify_voice_active -> millennium_audio_route_device
     |         +--> voiceware-trace.jsonl / audio-trace.jsonl
     |
     +--> millennium_telephony_device (bridge; command bytes per audio-routing-state-map)
     |         |
     |         +--> decode -> millennium_audio_route_device
     |         +--> status codes -> millennium_supervision_device
     |
     +--> millennium_audio_route_device (routing / mute state machines)
     |         |
     |         +--> handset / line / mic / earpiece state
     |         +--> interacts: modem (line present), hookswitch (off-hook)
     |
     +--> millennium_audio_device (alerter / tones)
     |         |
     |         +--> SPEAKER + optional stream OR trace-only cadence
     |
     +--> millennium_supervision_device
     |         |
     |         +--> supervision-trace.jsonl; modem/call-state hooks
     |
     +--> trace/evidence output -> evidence bundle dirs
     |
     v
optional MAME sound / `.lay` indicators (non-proof; traces are proof)
```

## Sound/output hooks

- **Primary proof**: structured traces (`audio-trace.jsonl`, `voiceware-trace.jsonl`, etc.) and boot/evidence bundles — see [`audio-evidence-bundle-plan.md`](audio-evidence-bundle-plan.md).
- **MAME `speaker_device` / mixer**: optional when wiring real PCM or tone generators; not required to prove firmware drove a port.
- **Artwork**: lamp/speaker icons may reflect trace-derived state; must not replace trace proof.

## Port/register binding placeholders (authoritative: JSON fixtures)

| Device | Binding | Spec |
| ------ | ------- | ---- |
| `millennium_voiceware_device` | Writes `0x40` (bit `0x08` reset), `0x42` bank `[3:0]`, `0x61` phrase | [`voiceware-mame-device-implementation.spec.md`](../specs/voiceware-mame-device-implementation.spec.md) |
| `millennium_telephony_device` | Transport attached under modem/host bridge — **no direct port list** in `audio-device-map.json` yet → **`compatibility_validation_required`** until `io-port-map.json` enumerates bridge decode | [`audio-routing-mame-device-implementation.spec.md`](../specs/audio-routing-mame-device-implementation.spec.md) |
| `millennium_audio_route_device` | Command bytes `0x20`–`0x46` effects from `audio-routing-state-map.json` **via telephony decode**, not arbitrary CPU ports | same |
| `millennium_supervision_device` | Status codes `0x60`–`0x8E` per `disconnect-supervision-map.json` `processor_to_host_codes` | [`disconnect-supervision-mame-device-implementation.spec.md`](../specs/disconnect-supervision-mame-device-implementation.spec.md) |
| `millennium_audio_device` | Alerter PIO/control bits — **OPEN** until `audio-device-map.json` gains `ports[]` for `alerter_path` | [`alerter-audio-mame-device-implementation.spec.md`](../specs/alerter-audio-mame-device-implementation.spec.md) |

## State machines (required enumerations — implement exactly)

| Device | State machine | States / notes |
| ------ | ------------- | ---------------- |
| Voiceware | Playback FSM | `VW_RESET_IDLE` → `VW_RESET_ASSERTED` → `VW_BANK_LATCHED` → `VW_ARMED` → `VW_PLAYING` → `VW_COMPLETING` → (`VW_IDLE` \| `VW_FAULT`) |
| Audio route | Call / mux FSM | `CALL_IDLE`, `CALL_DIALING`, `CALL_ESTABLISHED_*` (mirror `CALL_STATE_*` hex rows in routing map), plus orthogonal **TX/RX mute** and **sidetone** substates |
| Supervision | Disconnect FSM | `SUP_IDLE` → `SUP_CODES_STREAMING` → `SUP_DISCONNECT_PENDING` → `SUP_DISCONNECT_EMITTED` → `SUP_CLEARED`; parallel **watchdog** timer state `TW_RUN`/`TW_FIRED` |
| Alerter | Cadence FSM | `ALT_SILENT` → `ALT_ON_PHASE` → `ALT_OFF_PHASE` → … → `ALT_SILENT` per tone class table |

Exact edge predicates are specified per-device in [`../specs/*-mame-device-implementation.spec.md`](../specs/).

## Interaction matrix

| Source | Consumers |
| ------ | --------- |
| Modem / line presence | Audio route, supervision, optional voiceware gating |
| Hookswitch | Audio route (handset vs on-hook), supervision assumptions |
| Voiceware playback active | Audio route (prompt path), VFD text only via **firmware** writes to VFD — not injected |

## VFD / prompts

Display strings follow normal VFD device rules. **No parallel “fake voice state”** tied to UI: voice subsystem state comes only from voiceware device internal model updated by I/O.

## Related documents

- [`voiceware-emulation.md`](voiceware-emulation.md), [`audio-routing-emulation.md`](audio-routing-emulation.md), [`disconnect-supervision-emulation.md`](disconnect-supervision-emulation.md)
- [`audio-implementation-roadmap.md`](audio-implementation-roadmap.md)
- [`audio-boot-integration-plan.md`](audio-boot-integration-plan.md)
