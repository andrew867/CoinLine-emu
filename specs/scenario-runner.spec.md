# Spec — Scenario runner

This spec defines the JSON schema for scenario files under `fixtures/scenarios/`. See [`../docs/scenario-runner.md`](../docs/scenario-runner.md) for narrative context.

## Schema (informal)

```jsonc
{
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "type": "object",
  "required": ["scenario_id", "board_profile", "steps"],
  "properties": {
    "scenario_id":     { "type": "string", "pattern": "^[a-z0-9-]+$" },
    "description":     { "type": "string" },
    "firmware_sha256": { "type": "string", "pattern": "^[0-9a-f]{64}$" },
    "board_profile":   { "type": "string" },
    "nvram":           { "type": "string" },
    "host_bridge": {
      "type": "object",
      "properties": {
        "transport": { "enum": ["tcp", "websocket", "pipe", "serial"] },
        "endpoint":  { "type": "string" }
      }
    },
    "evidence_dir":    { "type": "string" },
    "seed":            { "type": "integer" },
    "steps": {
      "type": "array",
      "items": { "$ref": "#/$defs/step" },
      "minItems": 1
    }
  },
  "$defs": {
    "step": {
      "type": "object",
      "required": ["verb"],
      "properties": {
        "verb": {
          "enum": [
            "reset",
            "run_cycles",
            "wait_for_pc",
            "wait_for_milestone",
            "wait_for_display",
            "pause",
            "press_key",
            "lift_handset",
            "hang_up",
            "swipe_card",
            "insert_smartcard",
            "insert_coin",
            "set_lock_state",
            "set_door_state",
            "set_vault_state",
            "set_service_state",
            "connect_host",
            "disconnect_host",
            "inject_modem_event",
            "expect_uart_bytes",
            "expect_vfd_text",
            "expect_nvram_write",
            "expect_host_frame",
            "expect_milestone",
            "export_evidence"
          ]
        }
      }
    }
  }
}
```

## Verb-specific arguments

| Verb | Required arguments |
| ---- | ------------------ |
| `run_cycles` | `cycles: integer` |
| `wait_for_pc` | `pc: hex string`, `timeout_cycles: integer` |
| `wait_for_milestone` | `milestone: "M0".."M12"`, `timeout_cycles: integer` |
| `wait_for_display` | `rows: array of strings`, `timeout_cycles: integer` |
| `press_key` | `key: string`, `duration_cycles: integer` |
| `swipe_card` | `fixture: path` |
| `insert_smartcard` | `fixture: path` |
| `insert_coin` | `denomination: integer | "jam" | "reject"` |
| `set_*_state` | `value: boolean` |
| `inject_modem_event` | `event: "carrier_lost"|"ringing"|"busy"|"no_answer"|"noisy_line"` |
| `expect_uart_bytes` | `direction: "tx"|"rx"`, `hex: string` |
| `expect_vfd_text` | `rows: array of strings` |
| `expect_nvram_write` | `address: hex string`, `value: hex string` |
| `expect_host_frame` | `matcher: string` |
| `expect_milestone` | `milestone: "M0".."M12"` |

## Tests

- `tests/fixtures/test_scenario_schema.py` — schema validation for every shipped scenario.
- `tests/integration/test_scenario_runner_smoke.cpp` — runs a trivial scenario end-to-end.

## Cross-references

- [`../docs/scenario-runner.md`](../docs/scenario-runner.md).
- [`evidence-bundle.spec.md`](evidence-bundle.spec.md).
