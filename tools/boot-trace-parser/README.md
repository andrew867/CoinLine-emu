# boot-trace-parser

Summarizes `boot-trace.jsonl` from an evidence bundle (one JSON object per line, milestone schema in `docs/boot-milestones.md`).

## CLI

```text
boot-trace-parser [--input <path>] [--output <path>] [--filter <M#>] [--format table|json] [--quiet]
```

- **`--help`** — short usage and examples.
- **`--help-all`** — every flag.
- Input defaults to **stdin**; output defaults to **stdout**.
- **`--format json`** echoes filtered JSONL (stable field order as read).
- Errors go to **stderr** with exit code **1**.

## Examples

```bash
boot-trace-parser --input path/to/evidence/boot-trace.jsonl
boot-trace-parser --input path/to/evidence/boot-trace.jsonl --filter M10 --output summary.txt
```

## Agent notes

- Non-interactive; suitable for CI.
- Ill-formed lines (not a JSON object, or missing `milestone`) are rejected with a clear message and non-zero exit.
