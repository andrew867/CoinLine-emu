# Audio / supervision — evidence bundle plan

Extends [`evidence-bundles.md`](evidence-bundles.md).

## Mandatory artifacts when audio scenario flag set

When `scenario.json` / manifest sets **`expect_audio_traces: true`** (or scenario id matches `fixtures/scenarios/audio-*.json` / voice/supervision scenarios), the exporter **must** include every **non-optional** row below. Missing files → bundle validation failure.

| Path | Required | Description |
| ---- | -------- | ----------- |
| `audio/audio-trace.jsonl` | **yes** | Superset stream |
| `audio/voiceware-trace.jsonl` | **yes** | Voiceware-filtered |
| `audio/supervision-trace.jsonl` | **yes** | Supervision-filtered |
| `audio/alerter-trace.jsonl` | **yes** | Alerter-filtered; must contain at least **`alerter_ready`** after device reset (proves emitter wired) |
| `audio/audio-state-final.json` | **yes** | Snapshot: `{ route_state, mute_state, vw_state, sup_state, last_prompt_id }` |
| `boot-trace.jsonl` | **yes** | Existing ladder — unchanged |
| `io-trace.jsonl` | **yes** | Correlation for port-backed proof |

## Optional artifacts

| Path | Description |
| ---- | ----------- |
| `audio/audio-route-state-history.jsonl` | Sampled route history |
| `audio/prompt-playback-history.jsonl` | Prompt IDs + cycles |
| `audio/handset.wav`, `audio/alerter.wav` | PCM captures — **never** substitute for trace proof |

## Manifest additions

`manifest.json` **must** include when audio enabled:

```jsonc
"mame_executable": "build/bin/coinline-mame.exe",
"firmware_sha256": "<must match fixtures/firmware/firmware-hashes.json>",
"audio": {
  "expect_audio_traces": true,
  "audio_milestones_claimed": ["M6A"],
  "trace_sha256": { "audio-trace.jsonl": "...", "voiceware-trace.jsonl": "..." }
}
```

`mame_executable` path is the actual argv[0] used — proves bundle was not generated from a browser harness.

## Validation rules

- **`expect_audio_traces: true`** ⇒ all mandatory files present and non-empty **except** filtered traces may have zero events only when accompanied by `compatibility_validation_required` explanation in `scenario_result.json`.
- SHA-256 of firmware must match manifest.
- Milestone claims **M6A–M11D** require matching `event_type` rows in the cited trace files.

## Cross-reference

Schema evolution: [`specs/evidence-bundle.spec.md`](../specs/evidence-bundle.spec.md), trace format [`specs/audio-trace-format.spec.md`](../specs/audio-trace-format.spec.md).
