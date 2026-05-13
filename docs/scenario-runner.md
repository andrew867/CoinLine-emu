# Scenario runner

This document defines the JSON scenario format and the verb set understood by the scenario runner. The schema is in [`../specs/scenario-runner.spec.md`](../specs/scenario-runner.spec.md).

## Goal

Produce deterministic, automated runs of the emulator that:

- Drive firmware through known flows.
- Assert on observable state (PC, VFD buffer, NVRAM writes, host-bridge frames).
- Export an evidence bundle.

## File format

JSON-first. YAML support is optional and added only if downstream tooling already consumes it; the canonical format is JSON.

```jsonc
{
  "scenario_id": "boot-to-idle",
  "description": "Boot the supported firmware to the idle display.",
  "firmware_sha256": "...",
  "board_profile": "fixtures/board/board-profile-2line-vfd.json",
  "nvram": "fixtures/nvram/factory-default.nvram.json",
  "host_bridge": { "transport": "tcp", "endpoint": "tcp://127.0.0.1:5210" },
  "evidence_dir": "out/boot-to-idle/",
  "steps": [
    { "verb": "reset" },
    { "verb": "wait_for_milestone", "milestone": "M10", "timeout_cycles": 100000000 },
    { "verb": "expect_vfd_text", "rows": ["INSERT $1.00      ", "OR SWIPE CARD     "] },
    { "verb": "export_evidence" }
  ]
}
```

## Verbs

Every verb is described below. Verbs marked **device** dispatch through a device's fixture-injection interface (per [`device-model.md`](device-model.md)).

### Lifecycle

| Verb | Purpose |
| ---- | ------- |
| `reset` | Reset the machine. |
| `run_cycles` | Advance N CPU cycles. `{ "verb": "run_cycles", "cycles": 1000 }` |
| `wait_for_pc` | Run until PC reaches a value or timeout. `{ "verb": "wait_for_pc", "pc": "0x4321", "timeout_cycles": 1000000 }` |
| `wait_for_milestone` | Run until a boot milestone is reached. `{ "verb": "wait_for_milestone", "milestone": "M10", "timeout_cycles": 100000000 }` |
| `wait_for_display` | Run until VFD shows a target text. `{ "verb": "wait_for_display", "rows": ["INSERT..."], "timeout_cycles": 50000000 }` |
| `pause` | Pause for diagnostic inspection (no-op in CI). |

### Front-panel inputs (device verbs)

| Verb | Purpose |
| ---- | ------- |
| `press_key` | Press and release a key. `{ "verb": "press_key", "key": "5", "duration_cycles": 1000 }` |
| `lift_handset` | Off-hook. |
| `hang_up` | On-hook. |
| `swipe_card` | Inject a magstripe payload. `{ "verb": "swipe_card", "fixture": "fixtures/cards/magcard-valid.json" }` |
| `insert_smartcard` | Inject a smart card. `{ "verb": "insert_smartcard", "fixture": "fixtures/cards/smartcard-valid.json" }` |
| `insert_coin` | Trigger a coin event. `{ "verb": "insert_coin", "denomination": 25 }` |
| `set_lock_state` | `{ "verb": "set_lock_state", "value": false }` |
| `set_door_state` | `{ "verb": "set_door_state", "value": true }` |
| `set_vault_state` | `{ "verb": "set_vault_state", "value": true }` |
| `set_service_state` | `{ "verb": "set_service_state", "value": true }` |

### Host bridge

| Verb | Purpose |
| ---- | ------- |
| `connect_host` | Connect (or re-connect) the host bridge. |
| `disconnect_host` | Disconnect the host bridge. |
| `inject_modem_event` | `{ "verb": "inject_modem_event", "event": "carrier_lost" }` (also `ringing`, `busy`, `no_answer`, `noisy_line`) |

### Assertions

| Verb | Purpose |
| ---- | ------- |
| `expect_uart_bytes` | `{ "verb": "expect_uart_bytes", "direction": "tx", "hex": "..." }` |
| `expect_vfd_text` | `{ "verb": "expect_vfd_text", "rows": ["..."] }` |
| `expect_nvram_write` | `{ "verb": "expect_nvram_write", "address": "0x...", "value": "0x..." }` |
| `expect_host_frame` | `{ "verb": "expect_host_frame", "matcher": "ncc:dlog_submit" }` |
| `expect_milestone` | `{ "verb": "expect_milestone", "milestone": "M11" }` |

### Output

| Verb | Purpose |
| ---- | ------- |
| `export_evidence` | Flush an evidence bundle to `evidence_dir`. |

## Determinism

- Cycles, not wall-clock, drive timing.
- All fixtures are content-addressed.
- Host bridge transcripts are recorded; deterministic replays use a recorded transcript instead of a live peer.

## Failure handling

A scenario fails on the first assertion miss or timeout. The runner emits the evidence bundle even on failure, so triage has all the artifacts.

## Catalog

Initial scenarios:

| File | Purpose |
| ---- | ------- |
| `fixtures/scenarios/boot-to-idle.json` | Boot + idle. |
| `fixtures/scenarios/keypad-smoke.json` | Press every key once and verify firmware acknowledgment. |
| `fixtures/scenarios/modem-connect.json` | Initiate a modem call; verify host-bridge bytes. |
| `fixtures/scenarios/table-download.json` | Full table-download against running CoinLine Host. |
| `fixtures/scenarios/card-call.json` | Card swipe → host call → DLOG. |
| `fixtures/scenarios/coin-call.json` | Coin insertion → host call → DLOG. |
| `fixtures/scenarios/service-mode.json` | Service-mode entry. |

## Cross-references

- [`../specs/scenario-runner.spec.md`](../specs/scenario-runner.spec.md).
- [`evidence-bundles.md`](evidence-bundles.md).
- [`../test-plans/scenario-runner-tests.md`](../test-plans/scenario-runner-tests.md).
