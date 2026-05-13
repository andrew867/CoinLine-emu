# Spec — MAME machine driver

This spec defines the MAME machine driver contract for `coinline-emu`. It is normative for `src/mame/coinline/millennium*.cpp/h`.

## Identity

| Field | Value |
| ----- | ----- |
| Short-name | `millennium` |
| Source root | `src/mame/coinline/` |
| Driver entry | `millennium.cpp` |
| State class | `millennium_state` |
| Subtarget | `coinline` |
| Display name | "Millennium-compatible Z180 payphone terminal" |

## Required entry points

| Symbol | Purpose |
| ------ | ------- |
| `millennium_state::machine_start()` | Allocate per-machine resources; load board profile. |
| `millennium_state::machine_reset()` | Reset CPU and devices; clear NVRAM if `--clear-nvram`. |
| `millennium_state::memory_map(address_map &)` | Wire program-space regions per [`../docs/memory-map.md`](../docs/memory-map.md). |
| `millennium_state::io_map(address_map &)` | Wire I/O ports per [`../docs/io-port-map.md`](../docs/io-port-map.md); route unknown ports to `unknown_port_logger`. |
| `INPUT_PORTS_START(millennium)` | Declare keypad, hookswitch, security inputs, etc. |
| `MACHINE_CONFIG_START(millennium)` | Configure CPU, devices, layout, screen. |
| `ROM_START(millennium)` | Declare ROM region; firmware loaded at runtime via `-firmware`. |

## CLI flags

The driver must support these custom flags (in addition to MAME defaults):

| Flag | Required | Purpose |
| ---- | -------- | ------- |
| `-firmware <path>` | Yes | Path to `../firmware/flash.bin`. |
| `-board <profile>` | Yes | Path to `fixtures/board/board-profile-*.json`. |
| `-nvram <path>` | No | Initial NVRAM image. |
| `-hostbridge <url>` | No | Host bridge transport endpoint. |
| `-scenario <file>` | No | Scenario file to run automatically. |
| `-evidence <dir>` | No | Output directory for evidence bundles. |
| `-clear-nvram` | No | Initialize NVRAM from factory image instead of persisted file. |
| `-exit-on-scenario-end` | No | Exit when scenario completes (CI mode). |

## Behavior

| Behavior | Required |
| -------- | -------- |
| Halts on illegal opcode (Z180 trap) | Yes; logged. |
| Logs unknown I/O port reads/writes | Yes per [`../docs/io-port-map.md`](../docs/io-port-map.md). |
| Persists NVRAM across runs | Yes via MAME's NVRAM machinery. |
| Supports MAME debugger | Yes. |
| Supports save state | Yes. |

## Machine flags

`MACHINE_NOT_WORKING|MACHINE_NO_SOUND` initially. Removed per the schedule in [`../docs/mame-driver-plan.md`](../docs/mame-driver-plan.md).

## Acceptance

- Driver appears in `--list-machines`.
- Driver boots a known firmware to milestone M1 in CI.
- All board profiles load without schema errors.
- All CLI flags listed above are honored.

## Cross-references

- [`../docs/mame-driver-plan.md`](../docs/mame-driver-plan.md).
- [`../docs/architecture.md`](../docs/architecture.md).
