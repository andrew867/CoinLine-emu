# Audio routing — MAME implementation test plan

## Purpose

Validate **`millennium_audio_route_device`** and **`millennium_telephony_device`** decode path: route and mute machines, hook/modem/voice coupling, traces.

## Harness contract (normative)

| Class | Executable | Firmware proof? |
| ----- | ---------- | ----------------- |
| **A** | GoogleTest in-process only | **No** |
| **C** | **`coinline-mame.exe`** + firmware | **Yes** — requires correlated `telephony_command_decode` / `route_change` in traces from emulator run |

No browser harness. Scenario JSON must not embed target route enums.

## Test taxonomy

| Class | Description |
| ----- | ----------- |
| **A** | Fixture-driven unit tests — replay port writes from JSON |
| **B** | Integration without firmware proof — wiring smoke |
| **C** | **Real firmware** — hook/off-hook and route transitions in traces |

Scenario JSON **must not** embed target `route_state`; scenarios apply **physical inputs** (hook, keys) only.

## Prerequisites

Same as voiceware plan; board profile with routing map merged.

## Fixtures

| Fixture | Expectation |
| ------- | ----------- |
| `fixtures/audio/audio-route-idle.json` | Power-on default |
| `fixtures/audio/audio-route-offhook-prompt.json` | Off-hook + prompt path |
| `fixtures/audio/audio-route-call-connected.json` | Connected duplex |
| `fixtures/audio/audio-route-muted.json` | Mic/ear mute |

## Scenarios

- `fixtures/scenarios/mic-mute-unmute.json`
- `fixtures/scenarios/earpiece-mute-unmute.json`
- `fixtures/scenarios/line-to-earpiece-route.json`

## Commands

Same pattern as [`voiceware-mame-implementation-tests.md`](voiceware-mame-implementation-tests.md): build script, run script, test wrapper.

## Expected trace files

`audio-trace.jsonl` with `route_change`, `mute_change`.

## Pass criteria

- Class A: Golden route sequence hash matches.
- Class C: Correlation between hookswitch I/O (from `io-trace.jsonl`) and `route_change` events.

## Fail criteria

- Route state asserted from scenario without matching port activity.

## Source files touched

`millennium_audio_route.cpp/.h`, `millennium_telephony.cpp/.h`, `millennium_io.cpp/.h`, `millennium_keypad.cpp` (hook notify), `millennium_modem.cpp` (line notify), `millennium_hostbridge.cpp/.h` if bridge bytes attach there.

## Artifact outputs

Traces + bundle slice `audio/audio-state-final.json`.
