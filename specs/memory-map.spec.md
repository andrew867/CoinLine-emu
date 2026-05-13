# Spec — Memory map

This spec defines the JSON schema for `fixtures/board/memory-map.json`. See [`../docs/memory-map.md`](../docs/memory-map.md) for narrative context.

## Schema (informal)

```jsonc
{
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "type": "object",
  "required": ["address_spaces", "regions"],
  "properties": {
    "address_spaces": {
      "type": "array",
      "items": { "enum": ["program", "io"] },
      "minItems": 1
    },
    "regions": {
      "type": "array",
      "items": {
        "type": "object",
        "required": ["name", "base", "size", "access", "device"],
        "properties": {
          "name":   { "type": "string" },
          "base":   { "type": "string", "pattern": "^0x[0-9A-Fa-f]+$" },
          "size":   { "type": "integer", "minimum": 0 },
          "access": { "enum": ["r", "w", "rw"] },
          "device": { "type": "string" },
          "evidence": { "type": "string" },
          "notes":    { "type": "string" }
        }
      }
    }
  }
}
```

## Required regions

A valid memory map must contain at least these regions:

| Name | Required |
| ---- | -------- |
| `rom` | Yes |
| `ram` | Yes |
| `nvram` | Yes |
| `tablestore` | Yes (size may be 0 if not used by SKU) |
| `dlastage` | Yes (size may be 0 if not used by SKU) |

## Layout rules

- Regions must not overlap.
- `rom.base` must be `0x00000` (Z180 reset vector requirement).
- All bases must be aligned to at least the page boundary used by the MMU.

## Tests

- `tests/fixtures/test_memory_map_schema.py` — schema validation.
- `tests/fixtures/test_memory_map_layout.py` — overlap + alignment + required regions.

## Cross-references

- [`../docs/memory-map.md`](../docs/memory-map.md).
- [`z180-board-profile.spec.md`](z180-board-profile.spec.md).
