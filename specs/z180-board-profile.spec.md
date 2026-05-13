# Spec — Z180 board profile

This spec defines the JSON schema for board profiles under `fixtures/board/board-profile-*.json`. See [`../docs/board-profiles.md`](../docs/board-profiles.md) for narrative context.

## Schema (informal)

```jsonc
{
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "type": "object",
  "required": [
    "profile_id", "display", "keypad", "card_reader", "smartcard",
    "coin", "alerter", "security", "memory", "z180", "modem",
    "host_bridge", "io_ports", "artwork"
  ],
  "properties": {
    "profile_id": { "type": "string", "pattern": "^[a-z0-9-]+$" },
    "display": {
      "type": "object",
      "required": ["type", "variant", "columns", "rows"],
      "properties": {
        "type":    { "const": "vfd" },
        "variant": { "enum": ["2line", "11line"] },
        "columns": { "type": "integer", "minimum": 1 },
        "rows":    { "type": "integer", "minimum": 1 }
      }
    },
    "keypad": {
      "type": "object",
      "required": ["layout"],
      "properties": {
        "layout":            { "enum": ["3x4", "4x4"] },
        "quick_access_keys": { "type": "array", "items": { "type": "string" } },
        "volume_up":         { "type": "string" },
        "volume_down":       { "type": "string" },
        "language":          { "type": "string" }
      }
    },
    "card_reader": {
      "type": "object",
      "required": ["present"],
      "properties": {
        "present": { "type": "boolean" },
        "type":    { "enum": ["magnetic", "none"] }
      }
    },
    "smartcard": {
      "type": "object",
      "required": ["present"],
      "properties": {
        "present": { "type": "boolean" },
        "type":    { "enum": ["iso7816-memory", "iso7816-microprocessor", "none"] }
      }
    },
    "coin": {
      "type": "object",
      "required": ["validator_type", "denominations"],
      "properties": {
        "validator_type": { "enum": ["pulse", "serial", "none"] },
        "denominations":  { "type": "array", "items": { "type": "integer" } }
      }
    },
    "alerter":   { "type": "object", "properties": { "present": { "type": "boolean" } } },
    "security": {
      "type": "object",
      "required": ["lock", "door", "vault", "service_switch"],
      "properties": {
        "lock":           { "type": "boolean" },
        "door":           { "type": "boolean" },
        "vault":          { "type": "boolean" },
        "service_switch": { "type": "boolean" }
      }
    },
    "memory": {
      "type": "object",
      "required": ["rom_size", "ram_size", "nvram_base", "nvram_size"],
      "properties": {
        "rom_size":           { "type": "integer", "minimum": 1024 },
        "ram_size":           { "type": "integer", "minimum": 1024 },
        "nvram_base":         { "type": "string", "pattern": "^0x[0-9A-Fa-f]+$" },
        "nvram_size":         { "type": "integer", "minimum": 64 },
        "table_storage_base": { "type": "string", "pattern": "^0x[0-9A-Fa-f]+$" },
        "table_storage_size": { "type": "integer", "minimum": 0 },
        "dla_stage_base":     { "type": "string", "pattern": "^0x[0-9A-Fa-f]+$" },
        "dla_stage_size":     { "type": "integer", "minimum": 0 }
      }
    },
    "z180": {
      "type": "object",
      "required": ["clock_hz"],
      "properties": {
        "clock_hz":    { "type": "integer", "minimum": 1000000 },
        "wait_states": {
          "type": "object",
          "properties": {
            "rom": { "type": "integer", "minimum": 0, "maximum": 7 },
            "ram": { "type": "integer", "minimum": 0, "maximum": 7 },
            "io":  { "type": "integer", "minimum": 0, "maximum": 7 }
          }
        }
      }
    },
    "modem": {
      "type": "object",
      "required": ["asci_channel", "default_baud"],
      "properties": {
        "asci_channel": { "enum": [0, 1] },
        "default_baud": { "type": "integer" },
        "default_dcd":  { "type": "boolean" },
        "default_cts":  { "type": "boolean" }
      }
    },
    "host_bridge": {
      "type": "object",
      "properties": {
        "transport":        { "enum": ["tcp", "websocket", "pipe", "serial"] },
        "default_endpoint": { "type": "string" }
      }
    },
    "io_ports": {
      "type": "object",
      "properties": {
        "on_unknown":       { "enum": ["log_and_continue", "log_and_halt"] },
        "unknown_default":  { "type": "string", "pattern": "^0x[0-9A-Fa-f]{2}$" }
      }
    },
    "artwork": {
      "type": "object",
      "properties": {
        "front_image": { "type": "string" },
        "layout":      { "type": "string" }
      }
    },
    "firmware": {
      "type": "object",
      "properties": {
        "strict_hash":       { "type": "boolean" },
        "supported_versions": { "type": "array", "items": { "type": "string" } }
      }
    }
  }
}
```

## Cross-fixture rules

| Rule | Validation |
| ---- | ---------- |
| `nvram_base + nvram_size` must not overlap `table_storage_base..+table_storage_size`. | `tests/fixtures/test_board_profile_layout.py`. |
| `memory.rom_size` must equal the firmware binary's size. | `tests/boot/test_firmware_load.cpp` cross-check. |
| `display.variant` and `keypad.quick_access_keys` must be consistent (e.g., 11-line variant has additional keys). | `tests/fixtures/test_board_profile_consistency.py`. |
| `host_bridge.transport == "serial"` requires a path; `"tcp"`/`"websocket"` requires `host:port`. | Schema-level validation. |

## Tests

- `tests/fixtures/test_board_profile_schema.py` — schema validation.
- `tests/fixtures/test_board_profile_layout.py` — layout consistency.
- `tests/fixtures/test_board_profile_consistency.py` — cross-field consistency.

## Cross-references

- [`../docs/board-profiles.md`](../docs/board-profiles.md).
- [`memory-map.spec.md`](memory-map.spec.md).
- [`io-port-map.spec.md`](io-port-map.spec.md).
