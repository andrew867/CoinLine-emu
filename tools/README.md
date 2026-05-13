# coinline-emu tools

Agent-oriented CLIs for evidence bundles and recorded traces. Each tool has its own `README.md`, `--help` / `--help-all`, non-interactive operation, and stderr + non-zero exit on errors.

| Directory | Binary | Purpose |
| --------- | ------ | ------- |
| `boot-trace-parser/` | `boot-trace-parser` | Summarize `boot-trace.jsonl`. |
| `io-trace-analyzer/` | `io-trace-analyzer` | Filter / histogram `io-trace.jsonl`. |
| `evidence-bundle-export/` | `evidence-bundle-export` | Pull framed JSON from a TCP debug port and write a bundle. |

## Build

From ``:

```bash
cmake -S . -B build -G Ninja
cmake --build build
```

Executables are emitted next to other targets in the CMake build directory (for example `build/boot-trace-parser`).

## Tests

```bash
ctest --test-dir build --label-regex tool
```
