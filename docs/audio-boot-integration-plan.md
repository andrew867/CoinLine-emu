# Audio / supervision — boot integration plan

## Principle

Boot milestones **M0–M10** remain defined in [`boot-milestones.md`](boot-milestones.md). Audio-specific extensions (**M6A–M11D**) **add** evidence lines; they do not replace M6–M10.

## Trace emission points

| Milestone | When emitted | Required signal |
| --------- | ------------- | ---------------- |
| M6A | First voiceware-related write sequence completes (profile-defined) | `voiceware-trace.jsonl` + optional boot-trace extension |
| M6B | Audio route device reports default power-on route | `audio-trace.jsonl` `route_state` |
| M6C | Alerter idle / ready | `alerter-trace.jsonl` or io-trace |
| M8A | Supervision device initialized (reads or enables per map) | `supervision-trace.jsonl` |
| M10A | Idle **audio route** matches fixture `audio-route-idle.json` | trace + fixture compare |
| M11A | Off-hook transition observed | hookswitch + route trace correlation |
| M11B | Voice prompt playback cycle observed | voiceware complete event |
| M11C | Mic mute/unmute | route trace mute bits |
| M11D | Disconnect supervision event | supervision trace event type |

## Boot trace vs dedicated traces

- **`boot-trace.jsonl`**: high-level milestone ladder (existing schema).
- **`audio-*.jsonl`**: fine-grained per [`audio-trace-format.spec.md`](../specs/audio-trace-format.spec.md).

Aggregator tools (Tranche A6) may **derive** summary JSON (`audio-state-final.json`) for bundles.

## Environment variables (existing pattern)

Reuse `COINLINE_*` paths from [`RUNNING.md`](../RUNNING.md) / run scripts. Add when implementing:

- `COINLINE_VOICE_TRACE`, `COINLINE_AUDIO_TRACE`, `COINLINE_SUPERVISION_TRACE`, `COINLINE_ALERTER_TRACE` — optional paths for dedicated JSONL streams.

## Failure triage

| Symptom | Check |
| ------- | ----- |
| No M6A | Voiceware ports not mapped or wrong reset timing |
| No M11D | Supervision model incomplete vs `disconnect-supervision-map.json` |
| M10 passes but M10A fails | Route default mismatch — update fixture or profile, not firmware |
