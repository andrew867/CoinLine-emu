# io-trace-analyzer

Filters `io-trace.jsonl` from an evidence bundle and optionally emits a per-port **histogram** (deterministic ordering by numeric port).

## CLI

```text
io-trace-analyzer [--input <path>] [--output <path>] [--port 0xNN] [--rw r|w]
                  [--device <name> --port-map <path>] [--histogram] [--format table|json] [--quiet]
```

- **`--help`** / **`--help-all`** — layered help.
- **`--device`** resolves ports from the board profile’s I/O map JSON (see `fixtures/board/io-port-map.json`); **`--port-map`** is required with **`--device`**.
- Invalid lines (not a JSON object, missing `port`) → stderr message and exit **1**.

## Examples

```bash
io-trace-analyzer --input path/to/evidence/io-trace.jsonl --port 0x60
io-trace-analyzer --input path/to/evidence/io-trace.jsonl --device vfd --port-map fixtures/board/io-port-map.json --histogram
```

## Agent notes

- No prompts; stdout is the only success channel.
- Port normalization is case-insensitive for the `0x` prefix and hex digits.
