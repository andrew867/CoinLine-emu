# Audio / supervision — implementation roadmap


**Validation boundary:** Conformance evidence is JSONL traces from **`coinline-mame.exe`** execution with real firmware, or Class A in-process device tests that are **explicitly not** labeled firmware proof — never browser-side audio checks. See [`audio-mame-device-architecture.md`](audio-mame-device-architecture.md) and [`audio-ci-plan.md`](audio-ci-plan.md).

## Goals

- Implement **MAME `device_t` subclasses** for voiceware, audio routing, and disconnect supervision; extend alerter/audio as specified.
- Drive **all** behavior from firmware port activity + documented timers (no scenario-injected internal audio state).
- Produce **trace files** per [`specs/audio-trace-format.spec.md`](../specs/audio-trace-format.spec.md).
- Integrate **evidence bundles** and **boot milestones** (M6A–M11D).

## Tranche summary

| Tranche | Theme | Exit criterion |
| ------- | ----- | -------------- |
| **A0** | Map validation | JSON maps validated vs `io-port-map`; gaps → compatibility items |
| **A1** | Voiceware device | `millennium_voiceware_device`; traces; unit + MAME smoke |
| **A2** | Routing + mute | `millennium_audio_route_device`; route traces |
| **A3** | Alerter output | Extended `millennium_audio_device`; sound or trace cadence |
| **A4** | Disconnect supervision | `millennium_supervision_device`; supervision traces |
| **A5** | Call-state integration | Devices agree on hook/line/voice/modem; scenarios |
| **A6** | Evidence bundles | Audio artifacts in bundle layout |
| **A7** | CI regression | Firmware-run tests; skip if firmware absent — never fake pass |

## Unresolved map items (do not guess hardware)

Items marked **`compatibility_validation_required`** or missing ports in [`fixtures/board/audio-device-map.json`](../fixtures/board/audio-device-map.json) remain **explicit validation rows** until traces or lab measurements promote them. Unknown behavior → [`compatibility-validation-items.md`](compatibility-validation-items.md), not guessed code paths.

## Dependencies

- [`fixtures/board/io-port-map.json`](../fixtures/board/io-port-map.json), [`audio-device-map.json`](../fixtures/board/audio-device-map.json), [`voiceware-command-map.json`](../fixtures/board/voiceware-command-map.json), [`audio-routing-state-map.json`](../fixtures/board/audio-routing-state-map.json), [`disconnect-supervision-map.json`](../fixtures/board/disconnect-supervision-map.json)
- Working **MAME build** (`tools/windows/build-mame-coinline.ps1`) and **firmware path** for execution tests

## Recommended starting tranche

**A0** — validate maps and freeze port bindings before writing device classes.
