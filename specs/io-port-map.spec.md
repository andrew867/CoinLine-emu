# Spec — I/O port map

This spec defines the JSON schema for `fixtures/board/io-port-map.json` and the unknown-port logging policy. See [`../docs/io-port-map.md`](../docs/io-port-map.md) for narrative context.

## Schema (informal)

```jsonc
{
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "type": "object",
  "required": ["unknown_default", "ports"],
  "properties": {
    "unknown_default":  { "type": "string", "pattern": "^0x[0-9A-Fa-f]{2}$" },
    "on_unknown":       { "enum": ["log_and_continue", "log_and_halt"] },
    "ports": {
      "type": "array",
      "items": {
        "type": "object",
        "required": ["port", "direction", "device", "status"],
        "properties": {
          "port":         { "type": "string", "pattern": "^0x[0-9A-Fa-f]{2}$" },
          "direction":    { "enum": ["r", "w", "rw"] },
          "active_level": { "enum": ["high", "low", "n/a"] },
          "device":       { "type": "string" },
          "evidence":     { "type": "string" },
          "handler":      { "type": "string" },
          "test":         { "type": "string" },
          "status":       { "enum": ["known", "suspected", "unknown"] },
          "default":      { "type": "string", "pattern": "^0x[0-9A-Fa-f]{2}$" }
        }
      }
    }
  }
}
```

## Unknown-port logging policy

When a port not declared with `status = "known"` or `status = "suspected"` is read or written:

- A structured log entry is emitted (see schema below).
- The read returns `unknown_default` (per board profile).
- The write is discarded.
- If `on_unknown = "log_and_halt"`, the CPU is halted after the log entry; otherwise it continues.

### Log entry schema

```jsonc
{
  "ts":            "string (RFC 3339 UTC)",
  "cycle":         "integer",
  "pc":            "string (hex)",
  "port":          "string (hex)",
  "rw":            "r | w",
  "value":         "string (hex)",
  "source_symbol": "string | null",
  "note":          "unknown_port | suspected_port | known_port"
}
```

## Required behavior

- Every emulator handler that owns a port must register it in the I/O port map fixture with `status = "known"`. Unregistered ports are unknown.

- The logger must include `source_symbol` if a symbol map is loaded.

## Tests

- `tests/fixtures/test_io_port_map_schema.py` — schema validation.
- `tests/devices/test_unknown_port_logging.cpp` — verifies log entry shape on unknown port read/write.
- `tests/devices/test_io_port_defaults.cpp` — verifies per-port defaults.

## Cross-references

- [`../docs/io-port-map.md`](../docs/io-port-map.md).

