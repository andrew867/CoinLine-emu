# Audio / supervision — debugging and trace plan

## Developer workflow

1. Build: `tools/windows/build-mame-coinline.ps1`
2. Run: `tools/windows/run-coinline-emulator.ps1 -FirmwareBinary <path> -RunSeconds N -Debug` as needed
3. Inspect **per-device JSONL** under `build/runs/<stamp>/` once emitters land

## Trace files

| File | Contents |
| ---- | -------- |
| `audio-trace.jsonl` | All audio subsystem events (superset stream) |
| `voiceware-trace.jsonl` | Voiceware-only |
| `supervision-trace.jsonl` | Supervision-only |
| `alerter-trace.jsonl` | Tone/buzzer events |

Schema: [`specs/audio-trace-format.spec.md`](../specs/audio-trace-format.spec.md).

## Correlation

- Join on **`cycle`** and **`pc`** with `io-trace.jsonl` and `boot-trace.jsonl`.
- Use **`port`** + **`decoded_command`** to verify firmware intent.

## Debugger hooks

Extend [`millennium_debug.cpp`](../src/mame/coinline/millennium_debug.cpp) only as needed for **non-invasive** logging flags (`-verbose` already); avoid printf storms in hot paths.

## Common defects

| Defect | Signature |
| ------ | --------- |
| Route stuck | `route_state` never leaves idle — hookswitch or modem inputs wrong |
| Voiceware hangs busy | Completion timer/IRQ not wired per profile |
| Supervision noise | CPC/timeout thresholds need compatibility item tuning |

## Binding authority

Port and command bindings live in **`fixtures/board/*.json`** (see [`voiceware-command-map.json`](../fixtures/board/voiceware-command-map.json), [`audio-routing-state-map.json`](../fixtures/board/audio-routing-state-map.json), [`disconnect-supervision-map.json`](../fixtures/board/disconnect-supervision-map.json)). These JSON maps are sufficient to implement the devices on their own.
