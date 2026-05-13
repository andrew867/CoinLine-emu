# Evidence bundles

This document defines the evidence bundle exported by `coinline-emu` for every scenario, acceptance, and host-integration test. The schema is in [`../specs/evidence-bundle.spec.md`](../specs/evidence-bundle.spec.md).

## Goal

Provide a single, self-contained directory that records exactly what the emulator did during a run, so that:

- A reviewer can confirm a compatibility item is satisfied.
- A developer can reproduce a bug.
- A regression test can replay against a recorded baseline.

## Bundle layout

```
out/<scenario_id>/
  manifest.json
  scenario.json
  scenario_result.json
  boot-trace.jsonl
  io-trace.jsonl
  uart-tx.hex
  uart-rx.hex
  vfd/
    final.json
    snapshots/<step_index>.json
  nvram/
    initial.json
    final.json
    diff.jsonl
  host-bridge/
    transcript.jsonl
    sideband.jsonl
  screenshots/
    <step_index>.png
  audio/
    audio-trace.jsonl
    voiceware-trace.jsonl
    supervision-trace.jsonl
    alerter-trace.jsonl
    audio-state-final.json
    audio-route-state-history.jsonl   (optional sampled history)
    prompt-playback-history.jsonl       (optional)
    handset.wav            (optional PCM evidence)
    alerter.wav            (optional PCM evidence)
  logs/
    emu.log
```

## `manifest.json`

```jsonc
{
  "schema_version": "1.0",
  "ts_start": "RFC 3339 UTC",
  "ts_end":   "RFC 3339 UTC",
  "scenario_id": "boot-to-idle",
  "emulator_version": "coinline-emu 0.0.0",
  "emulator_commit": "abcdef123",
  "engine_track": "mame|cleanroom",
  "firmware": {
    "path":   "../firmware/flash.bin",
    "size":   524288,
    "sha256": "..."
  },
  "board_profile": "fixtures/board/board-profile-2line-vfd.json",
  "host_bridge": {
    "transport": "tcp",
    "endpoint":  "tcp://127.0.0.1:5210"
  },
  "result": {
    "status":    "pass|fail",
    "milestone": "M10",
    "elapsed_cycles": 12345678,
    "fail_reason":   null
  }
}
```

## Per-file shape

| File | Shape |
| ---- | ----- |
| `boot-trace.jsonl` | One JSON object per milestone (M0–M12 schema in [`boot-milestones.md`](boot-milestones.md)). |
| `io-trace.jsonl` | One JSON object per I/O port read/write (schema in [`io-port-map.md`](io-port-map.md) → log entry schema). |
| `uart-tx.hex` / `uart-rx.hex` | Annotated hex with timestamps and cycle counts. |
| `vfd/final.json` | Final VFD buffer snapshot (schema in [`vfd-emulation.md`](vfd-emulation.md)). |
| `vfd/snapshots/<n>.json` | Snapshots taken at each `expect_vfd_text` step. |
| `nvram/initial.json`, `final.json` | Initial and final NVRAM images. |
| `nvram/diff.jsonl` | One write event per line `{ "address": "0x...", "value": "0x..." }`. |
| `host-bridge/transcript.jsonl` | Wire transcript per [`host-bridge.spec.md`](../specs/host-bridge.spec.md). |
| `host-bridge/sideband.jsonl` | Side-band frames if enabled. |
| `screenshots/<n>.png` | Front-panel screenshots at scenario steps. |
| `audio/audio-trace.jsonl` | Superset audio subsystem trace ([`specs/audio-trace-format.spec.md`](../specs/audio-trace-format.spec.md)). |
| `audio/voiceware-trace.jsonl` | Voiceware-filtered trace. |
| `audio/supervision-trace.jsonl` | Supervision-filtered trace. |
| `audio/alerter-trace.jsonl` | Alerter-filtered trace. |
| `audio/audio-state-final.json` | Summarized route/mute/prompt/supervision snapshot ([`audio-evidence-bundle-plan.md`](audio-evidence-bundle-plan.md)). |
| `audio/audio-route-state-history.jsonl` | Optional route history samples. |
| `audio/prompt-playback-history.jsonl` | Optional prompt playback history. |
| `audio/*.wav` | PCM captures (optional; traces remain authoritative). |
| `logs/emu.log` | Must include emulator argv proving **`coinline-mame.exe`** when audio conformance claimed. |

## Determinism guarantees

Two runs of the same scenario with the same firmware binary, board profile, NVRAM image, and host-bridge transcript produce evidence bundles that compare equal byte-for-byte (modulo timestamps in `manifest.json`). The emitter writes deterministic-ordered JSON.

## Compactness

For CI artifacts, `manifest.json`, `scenario_result.json`, and `boot-trace.jsonl` are kept; the rest is optional and subject to retention policy.

## Pass/fail summary

`manifest.json.result.status` is `pass` if and only if every assertion in the scenario passed. Any failure produces `status = "fail"` and `fail_reason` is populated.

## Tests

- `tests/integration/test_evidence_bundle.cpp` — verifies bundle layout, schema, and determinism for `boot-to-idle.json`.
- `tests/fixtures/test_evidence_bundle_schema.py` — JSON schema validation of all per-file shapes.

## Cross-references

- [`../specs/evidence-bundle.spec.md`](../specs/evidence-bundle.spec.md).
- [`boot-milestones.md`](boot-milestones.md).
- [`scenario-runner.md`](scenario-runner.md).
- [`debugging-guide.md`](debugging-guide.md).
