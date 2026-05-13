# Disconnect supervision — MAME implementation test plan

## Purpose

Validate **`millennium_supervision_device`**: disconnect typing, timeouts, status reads, interaction with modem/call teardown.

## Harness contract (normative)

| Class | Executable | Firmware proof? |
| ----- | ---------- | ----------------- |
| **A** | GoogleTest fixtures replaying status-code sequences | **No** |
| **C** | **`coinline-mame.exe`** + firmware + bridge/modem inject | **Yes** |

Browser validation forbidden. Scenarios inject **modem/bridge hex**, not internal `disconnect_event` registers.

## Test taxonomy

| Class | Firmware required for proof |
| ----- | ------------------------------ |
| **A** | No — timer + port replay from fixtures |
| **B** | Smoke |
| **C** | **Yes** — disconnect classification appears while CPU runs real binary |

External scenarios may inject **carrier drop** or **line bytes** at modem boundary — **not** internal supervision registers.

## Fixtures

| Fixture | Event type exercised |
| ------- | -------------------- |
| `fixtures/audio/disconnect-normal.json` | `normal_disconnect` |
| `fixtures/audio/disconnect-cpc.json` | `cpc` |
| `fixtures/audio/disconnect-timeout.json` | `timeout` |

## Scenario

- `fixtures/scenarios/disconnect-supervision.json`

## Commands

Build / run / test scripts per [`docs/RUNNING.md`](../docs/RUNNING.md).

## Expected traces

`supervision-trace.jsonl`, lines with `disconnect_event`.

## Pass criteria

- Class A: Fixture produces expected enum sequence.
- Class C: After host/carrier scenario phase, firmware observes disconnect via mapped reads **and** trace records classification **only if** profile defines CPC vs normal discrimination.

## Fail criteria

- Scenario sets `disconnect_event` field directly in a bypass channel.

## Source files touched

`millennium_supervision.cpp/.h`, `millennium_modem.cpp/.h`, `millennium_io.cpp/.h`

## Artifact outputs

`audio/supervision-trace.jsonl` in evidence bundle.
