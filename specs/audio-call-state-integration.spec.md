# Audio — call-state integration specification

## Purpose

Define **expected interactions** between audio devices across terminal **call states**. Evidence is **firmware-driven traces**, not UI assumptions.

## Call states

| State | Route expectation | Voice prompt | Mutes | Supervision |
| ----- | ----------------- | ------------ | ----- | ----------- |
| boot/init | Default idle route per fixture | May issue self-test tones | Usually open per profile | Idle |
| idle/on-hook | Idle route | Optional periodic none | Profile | Idle |
| off-hook | Off-hook route subset | Dial/recording prompts may arm | Mic live per map | Watching |
| dialing | Same as off-hook + DTMF path | Short tones optional | — | Watching |
| prompt playback | Prompt path active | Busy until complete | Profile may mute mic | Watching |
| connected call | Full duplex path per map | — | User mute toggles | Watching |
| muted call | Mute bits engaged | — | Mic and/or ear muted | Watching |
| card/coin prompt | Prompt path | Payment prompts | May mute mic | Watching |
| insufficient balance | Error tones + prompt | Error class | — | May arm disconnect |
| blocked call | Local tone | Error prompt | — | Local drop |
| service mode | Restricted route | Service prompts | Mic policy per map | Supervision relaxed |
| modem/download mode | Data path dominates audio mux | Minimal voice | May mute | Different watchdog |
| disconnect detected | Route teardown sequence | Stop prompts | Mutes reset per policy | Event fired |
| fault state | Safe idle route | Fault tones | Often muted | Fault classification |

## Firmware-driven evidence

Each row must be demonstrable via combined traces:

- `audio-trace.jsonl` cross-fields [`audio-trace-format.spec.md`](audio-trace-format.spec.md)
- Boot milestones M10A–M11D as applicable

## VFD interaction

VFD text changes are **not** used to assert audio — correlate only if firmware writes occur in same trace window.

## Trace proof matrix

Minimum events proving transition **into** `prompt playback`:

1. Voiceware `voice_segment_start`
2. Audio route `route_change` with prompt path
3. Optional `voice_segment_complete` exiting state

## Acceptance criteria

- [ ] Scenario [`fixtures/scenarios/audio-boot-init.json`](../fixtures/scenarios/audio-boot-init.json) reaches M6A/M6B when devices enabled — verified only via **`coinline-mame.exe`** traces + [`docs/audio-ci-plan.md`](../docs/audio-ci-plan.md) Class **C** rules.
- [ ] Disconnect scenario emits supervision event before route idle reset (same harness).
- [ ] No call-state row is marked **field-validated** or **CI-pass** without emulator executable path recorded in evidence manifest.
