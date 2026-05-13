# evidence-bundle-export

Connects to a **TCP** endpoint exposed by a running emulator (or a test mock), reads a **framed JSON** payload, and writes a complete evidence bundle directory using the same layout as `millennium_evidence_bundle_write()` (see `specs/evidence-bundle.spec.md`).

## Wire format (v1)

1. Client connects to `host:port`.
2. Server sends **4 bytes**: big-endian `uint32_t` length `N` of the JSON body.
3. Server sends **`N` bytes** of UTF-8 JSON (single object, no leading newline).

### JSON fields

| Field | Required | Notes |
| ----- | -------- | ----- |
| `scenario_id` | No | Default from payload. |
| `scenario_json_b64` | Yes | Base64 of `scenario.json` body. |
| `boot_trace_jsonl_b64` | Yes | Base64 of `boot-trace.jsonl` body. |
| `vfd_final_json_b64` | Yes | Base64 of `vfd/final.json` body. |
| `board_profile_relpath` | No | Relative path string. |
| `firmware_sha256` | No | Hex digest from the run. |
| `firmware_size` | Yes | Integer. |
| `firmware_path` | No | If omitted, manifest uses neutral label `emulator` (no `../` paths). |
| `deterministic_timestamps` | Yes | `true` / `false`. |
| `ts_start`, `ts_end` | No | RFC 3339 UTC strings. |
| `mame_executable` | When `expect_audio_traces` | Path to `coinline-mame.exe` actually run (`argv[0]`). |
| `expect_audio_traces` | No | When true, exporter requires mandatory `audio/*` traces + valid `firmware_sha256` + `mame_executable`. |
| `audio_trace_jsonl_b64`, `voiceware_trace_jsonl_b64`, … | No | Base64 of trace bodies from the emulator run (see wire schema in source). |

## CLI

```text
evidence-bundle-export --emulator <host:port> --output <dir> [--scenario <id>]
evidence-bundle-export --from-run-dir <run_dir> --output <dir> [--scenario <id>]
```

- **`--from-run-dir`** — Pack an existing `coinline-mame` / `run-screenshot-capture` output directory (reads `audio-trace.jsonl`, `evidence-summary.json`, etc.). No browser.
- **`--help`** / **`--help-all`** — layered help.
- Errors on **stderr**, exit code **1** on failure.

## Examples

```bash
evidence-bundle-export --emulator 127.0.0.1:9123 --output ./evidence/run-001
evidence-bundle-export --emulator 127.0.0.1:9123 --output ./evidence/run-001 --scenario boot-to-idle
```

## Agent notes

- Non-interactive; no stdin prompts.
- Payload size is capped (64 MiB) to avoid runaway allocations.
