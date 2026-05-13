# Spec — Evidence bundle

This spec defines the on-disk schema for evidence bundles. See [`../docs/evidence-bundles.md`](../docs/evidence-bundles.md) for narrative context.

## Bundle root

A bundle is a directory containing:

| Path | Required | Schema |
| ---- | -------- | ------ |
| `manifest.json` | Yes | see below |
| `scenario.json` | Yes | matches [`scenario-runner.spec.md`](scenario-runner.spec.md) |
| `scenario_result.json` | Yes | see below |
| `boot-trace.jsonl` | Yes | see below |
| `io-trace.jsonl` | Yes | see [`io-port-map.spec.md`](io-port-map.spec.md) |
| `uart-tx.hex`, `uart-rx.hex` | Yes | annotated hex |
| `vfd/final.json` | Yes | matches [`vfd-device.spec.md`](vfd-device.spec.md) |
| `vfd/snapshots/*.json` | Optional | per snapshot step |
| `nvram/initial.json`, `nvram/final.json` | Yes | matches [`nvram-storage-device.spec.md`](nvram-storage-device.spec.md) |
| `nvram/diff.jsonl` | Yes | one write per line |
| `host-bridge/transcript.jsonl` | Yes | matches [`host-bridge.spec.md`](host-bridge.spec.md) |
| `host-bridge/sideband.jsonl` | Optional | matches [`host-bridge.spec.md`](host-bridge.spec.md) |
| `screenshots/*.png` | Optional | PNG |
| `audio/*.wav` | Optional | WAV |
| `audio/audio-trace.jsonl` | **Yes** when `audio.expect_audio_traces` | [`audio-trace-format.spec.md`](audio-trace-format.spec.md) superset |
| `audio/voiceware-trace.jsonl` | **Yes** when audio enforced | filtered voiceware rows |
| `audio/supervision-trace.jsonl` | **Yes** when audio enforced | supervision-filtered |
| `audio/alerter-trace.jsonl` | **Yes** when audio enforced | must include `alerter_ready` after reset when emitted by firmware |
| `audio/audio-state-final.json` | **Yes** when audio enforced | route/mute/vw/sup snapshot |
| `logs/emu.log` | Yes | Must reference **`coinline-mame.exe` argv** when audio conformance is claimed |

## `manifest.json`

```jsonc
{
  "schema_version":   "1.0",
  "ts_start":         "RFC 3339 UTC",
  "ts_end":           "RFC 3339 UTC",
  "scenario_id":      "string",
  "emulator_version": "string",
  "emulator_commit":  "string",
  "engine_track":     "mame|cleanroom",
  "mame_executable":  "path/to/coinline-mame.exe",
  "firmware_sha256":  "hex64",
  "firmware": {
    "path":   "string",
    "size":   "integer",
    "sha256": "hex64"
  },
  "board_profile": "path",
  "host_bridge": {
    "transport": "tcp|websocket|pipe|serial",
    "endpoint":  "string"
  },
  "audio": {
    "expect_audio_traces": true,
    "audio_milestones_claimed": [],
    "trace_sha256": {}
  },
  "result": {
    "status":         "pass|fail",
    "milestone":      "M0..M12",
    "elapsed_cycles": "integer",
    "fail_reason":    "string|null"
  }
}
```

When **`audio.expect_audio_traces`** is true (or scenarios under [`audio-evidence-bundle-plan.md`](../docs/audio-evidence-bundle-plan.md)):

- **`mame_executable`** — required; the resolved **`argv[0]`** used to launch `coinline-mame.exe` (proves the bundle is not from a browser harness alone).
- **`firmware_sha256`** — duplicate at manifest root (same digest as `firmware.sha256`) for auditors scanning JSON without nesting.
- **`audio.trace_sha256`** — SHA-256 hex digests of the mandatory trace bodies (`audio-trace.jsonl`, `voiceware-trace.jsonl`, `supervision-trace.jsonl`, `alerter-trace.jsonl`).

## `scenario_result.json`

```jsonc
{
  "scenario_id": "string",
  "steps": [
    {
      "index": 0,
      "verb":  "wait_for_milestone",
      "status": "pass|fail|skipped",
      "actual": { ... },
      "expected": { ... },
      "elapsed_cycles": 123
    }
  ]
}
```

## `boot-trace.jsonl`

One JSON object per milestone matching the boot-trace schema in [`../docs/boot-milestones.md`](../docs/boot-milestones.md).

## `nvram/diff.jsonl`

```jsonc
{ "ts": "...", "cycle": 12345, "address": "0x...", "value": "0x..", "size": 1 }
```

## Determinism

The bundle directory layout is fixed; JSON is emitted in deterministic key order; timestamps and cycle counts are the only sources of non-determinism between two otherwise-identical runs.

## Tests

- `tests/fixtures/test_evidence_bundle_schema.py` — JSON schema validation.
- `tests/integration/test_evidence_bundle.cpp` — round-trip + determinism for `boot-to-idle.json`.

## Cross-references

- [`../docs/evidence-bundles.md`](../docs/evidence-bundles.md).
- [`scenario-runner.spec.md`](scenario-runner.spec.md).
