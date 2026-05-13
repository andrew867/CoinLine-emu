# Debugging guide

This document describes the debug workflow for `coinline-emu`. It complements [`evidence-bundles.md`](evidence-bundles.md) and [`scenario-runner.md`](scenario-runner.md).

## Common debug entry points

| Symptom | First step |
| ------- | ---------- |
| Boot stops before M1 | Verify `../firmware/flash.bin`, board profile, ROM region size. |
| Boot stops at M1–M4 | Inspect `boot-trace.jsonl`; cross-check the firmware evidence inventory and the firmware evidence inventory. |
| Boot stops at M5–M9 | Inspect `io-trace.jsonl`; cross-check the device source-maps. |
| Idle missing | Compare `vfd/final.json` against `fixtures/display/vfd-*-idle.json`. |
| Modem connect fails | Inspect `uart-tx.hex` and `host-bridge/transcript.jsonl`. |
| Table download fails | Inspect `nvram/diff.jsonl` and `host-bridge/transcript.jsonl`. |
| Audio milestone missing (M6A–M11D) | Inspect `audio/*.jsonl` per [`audio-debugging-and-trace-plan.md`](audio-debugging-and-trace-plan.md); correlate `io-trace.jsonl` ports with [`fixtures/board/audio-device-map.json`](../fixtures/board/audio-device-map.json). |

## Tools

| Tool | Purpose |
| ---- | ------- |
| `tools/boot-trace-parser/` | Pretty-print and summarize `boot-trace.jsonl` (see `tools/boot-trace-parser/README.md`). |
| `tools/io-trace-analyzer/` | Filter `io-trace.jsonl` by port or device; optional histogram (see `tools/io-trace-analyzer/README.md`). |
| `tools/evidence-bundle-export/` | Connect to a TCP debug port, read framed JSON, write a full evidence bundle (see `tools/evidence-bundle-export/README.md`). |

### Reproducible examples (after `cmake --build` on your `build/` directory)

Replace `$BUILD` with your CMake build output directory (for example `build`).

```bash
$BUILD/boot-trace-parser --help
$BUILD/boot-trace-parser --input tools/boot-trace-parser/tests/fixtures/sample-boot-trace.jsonl --output /tmp/boot-summary.txt
cat /tmp/boot-summary.txt

$BUILD/io-trace-analyzer --help
$BUILD/io-trace-analyzer --input tools/io-trace-analyzer/tests/fixtures/sample-io-trace.jsonl --port 0x60 --output /tmp/io-filtered.txt
$BUILD/io-trace-analyzer --input tools/io-trace-analyzer/tests/fixtures/sample-io-trace.jsonl --histogram --output /tmp/io-hist.txt

$BUILD/evidence-bundle-export --help
# Export requires a live TCP server speaking the framed JSON protocol (see tools/evidence-bundle-export/README.md).
# Smoke coverage runs via: ctest --test-dir $BUILD --label-regex tool
```

## MAME debugger

When the engine track is MAME, the MAME debugger is available with `-debug`:

| Command | Purpose |
| ------- | ------- |
| `bp <addr>` | Break at PC. |
| `wpset <addr>,<size>,r|w|rw` | Watchpoint on memory. |
| `g` | Continue. |
| `s` | Step one instruction. |
| `over` | Step over. |
| `trace <file>` | Turn on instruction tracing. |
| `print <reg>` | Print register or expression. |
| `dump <addr>,<size>` | Memory dump. |

## Symbol / map loading

If a symbol or map file is available, `millennium_debug.cpp` loads it and tags the boot/IO trace entries with `source_symbol`. Symbol files are not committed; they are loaded from `--symbols <path>` at runtime.

## Replay from evidence

```bash
./coinline-emu \
    -firmware ../firmware/flash.bin \
    -board fixtures/board/board-profile-2line-vfd.json \
    -replay out/boot-to-idle/host-bridge/transcript.jsonl \
    -nvram out/boot-to-idle/nvram/initial.json \
    -evidence out/replay/
```

Replay forces deterministic re-execution given the recorded inputs; useful to bisect a bug.

## Reproducing intermittent failures

- Set `--seed <n>` to fix any pseudo-random behavior.
- Use `wait_for_pc` instead of `run_cycles` to reach known states deterministically.
- Capture and replay the host-bridge transcript instead of running against a live peer.

## When to escalate to internal docs

If the public docs do not provide enough information to triage:

1. Open `` for evidence references.
2. Cross-check against `../firmware source tree` paths cited in those documents.

## Cross-references

- [`evidence-bundles.md`](evidence-bundles.md).
- [`scenario-runner.md`](scenario-runner.md).
- [`internal/`](internal/).
- [`boot-milestones.md`](boot-milestones.md) → failure modes table.

