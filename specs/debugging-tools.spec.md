# Spec — Debugging tools

This spec defines the contract for the CLI debugging tools under `tools/`. See [`../docs/debugging-guide.md`](../docs/debugging-guide.md) for context.

## Tools

| Tool | Path | Inputs | Outputs |
| ---- | ---- | ------ | ------- |
| Boot trace parser | `tools/boot-trace-parser/` | `boot-trace.jsonl` | Human-readable summary; tabular output. |
| I/O trace analyzer | `tools/io-trace-analyzer/` | `io-trace.jsonl` | Filtered subset; per-port histogram; PC heatmap. |
| Evidence bundle exporter | `tools/evidence-bundle-export/` | A live emulator state | A complete evidence bundle on disk. |

## Common CLI conventions

All tools:

- Accept `--help` and print usage with examples.
- Accept `--input <path>` and `--output <path>` (or stdin/stdout if absent).
- Emit JSON or pretty-table by default; switch with `--format json|table`.
- Exit non-zero on error; print error to stderr.
- Support `--quiet` for CI use.
- Are agent-friendly: layered help, predictable structure, no interactive prompts.

## Boot trace parser

```
Usage: boot-trace-parser [--input <path>] [--filter <milestone>]

Examples:
  boot-trace-parser --input out/boot/boot-trace.jsonl
  boot-trace-parser --input out/boot/boot-trace.jsonl --filter M5
```

Output:

```
M0 firmware loaded sha256=ab12... size=524288
M1 reset vector pc=0x0000
...
```

## I/O trace analyzer

```
Usage: io-trace-analyzer [--input <path>] [--port 0xA0] [--device vfd] [--rw r|w] [--histogram]
```

Examples:

```
io-trace-analyzer --input out/boot/io-trace.jsonl --port 0xA0
io-trace-analyzer --input out/boot/io-trace.jsonl --device vfd --histogram
```

## Evidence bundle exporter

```
Usage: evidence-bundle-export --emulator <socket> --output <dir> [--scenario <id>]
```

Connects to a running emulator's debug socket and dumps a complete bundle.

## Tests

- `tools/boot-trace-parser/tests/test_parser.cpp` (CTest: `test_boot_trace_parser_tool`, label `tool`)
- `tools/io-trace-analyzer/tests/test_filter.cpp` (CTest: `test_io_trace_analyzer_tool`, label `tool`)
- `tools/evidence-bundle-export/tests/test_export.cpp` (CTest: `test_evidence_bundle_export_tool`, label `tool`)
- CLI `--help` smoke tests (`boot_trace_parser_help`, `io_trace_analyzer_help`, `evidence_bundle_export_help`, label `tool`)

## Cross-references

- [`../docs/debugging-guide.md`](../docs/debugging-guide.md).
- [`evidence-bundle.spec.md`](evidence-bundle.spec.md).
