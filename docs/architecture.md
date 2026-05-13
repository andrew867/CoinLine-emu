# Architecture

This document describes the runtime architecture of `coinline-emu`. It is engine-agnostic in description and engine-specific only where called out (the MAME track is the default; see [`engine-selection.md`](engine-selection.md)).

## High-level diagram

```
+------------------------------------------------------------+
|              CoinLine Host (MIT, separate process)         |
|  - HostPlatform.Api / Server                                |
|  - DLOG, NCC, Tables, Rating, Cards, Firmware, Craft       |
+------------------------------+-----------------------------+
                               |
                               | TCP / WebSocket / named pipe / serial
                               | (UART bytes only — no source coupling)
                               |
+------------------------------+-----------------------------+
|         CoinLine Terminal Emulator (GPL-isolated)          |
|                                                            |
|   +----------------------------------------------------+   |
|   |  Frontend / artwork (MAME .lay or native overlay)  |   |
|   |  - VFD overlay (2-line / 11-line)                  |   |
|   |  - Clickable keypad, hookswitch, card slot, coin   |   |
|   |    input, lock/door/vault/service                   |   |
|   +----------------------------------------------------+   |
|                                                            |
|   +----------------------------------------------------+   |
|   |  Machine driver (millennium.cpp)                   |   |
|   |  +----------------------+  +---------------------+ |   |
|   |  |   Z180 CPU + MMU     |  | Memory map / I/O    | |   |
|   |  |   ASCI, PRT, INT,    |  | Address decoder     | |   |
|   |  |   DMA, wait states   |  | Unknown-port logger | |   |
|   |  +----------+-----------+  +----------+----------+ |   |
|   |             |                          |            |   |
|   |     +-------+--------------------------+--------+   |   |
|   |     |              Device tree                  |   |   |
|   |     |  VFD  Keypad  Hook  Card  Smartcard  Coin |   |   |
|   |     |  Modem-UART  NVRAM  Alerter  Security     |   |   |
|   |     +-------+--------------------------+--------+   |   |
|   |             |                          |            |   |
|   |     +-------+----------+      +--------+--------+   |   |
|   |     | Host bridge      |      | Scenario runner |   |   |
|   |     | (UART <-> TCP)   |      | + evidence bundles|  |   |
|   |     +------------------+      +-----------------+   |   |
|   +----------------------------------------------------+   |
|                                                            |
|     ROM region: ../firmware/flash.bin                   |
+------------------------------------------------------------+
```

The diagram makes the GPL/MIT process boundary explicit: only UART bytes (and side-channel scenario control messages) cross between `coinline-emu` and `coinline`. There is no shared library between the two trees.

## Layered view

| Layer | Responsibility | Where it lives |
| ----- | -------------- | -------------- |
| Frontend / artwork | Render the front panel; route input events | `artwork/` + MAME UI (or native overlay in MIT-clean track) |
| Machine driver | Compose CPU, memory, I/O, and the device tree | `src/mame/coinline/millennium*.cpp` |
| CPU | Execute Z180 instructions; expose registers, MMU, ASCI, PRT, INT, DMA | MAME `cpu/z180` (MAME track) or permissive Z80 + custom Z180 glue (MIT-clean track) |
| Memory map | Decode ROM/RAM/NVRAM/banks/vectors/stack/heap/I/O regions | `src/mame/coinline/millennium_memory.cpp`; spec in [`memory-map.md`](memory-map.md) |
| I/O map | Route reads/writes to devices; log unknown ports | `src/mame/coinline/millennium_io.cpp`; spec in [`io-port-map.md`](io-port-map.md) |
| Devices | One module per peripheral, contract per [`device-model.md`](device-model.md) | `src/mame/coinline/millennium_*.cpp` |
| Host bridge | Move UART bytes to/from a transport endpoint | `src/mame/coinline/millennium_hostbridge.cpp`; spec in [`modem-uart-host-bridge.md`](modem-uart-host-bridge.md) |
| Scenario runner | Drive the machine deterministically | `tools/` + scenario JSON; spec in [`scenario-runner.md`](scenario-runner.md) |
| Evidence bundles | Export traces, screenshots, NVRAM dumps, transcripts | `tools/evidence-bundle-export`; spec in [`evidence-bundles.md`](evidence-bundles.md) |
| Debug tools | Boot trace parser, I/O trace analyzer, symbol/map loader | `tools/` |

## Boot flow

```
+--------------+      +--------------+      +--------------+      +--------------+
| firmware     |      | reset vector |      | startup code |      | RAM init     |
| binary       +----->+ executed     +----->+ begins       +----->+              |
+--------------+      +--------------+      +--------------+      +------+-------+
                                                                          |
                                                                          v
+--------------+      +--------------+      +--------------+      +--------------+
| Z180         |      | first        |      | VFD writes   |      | keypad scan  |
| registers    +----->+ device init  +----->+              +----->+              |
| initialized  |      | observed     |      |              |      |              |
+--------------+      +--------------+      +--------------+      +------+-------+
                                                                          |
                                                                          v
+--------------+      +--------------+      +--------------+      +--------------+
| UART/modem   |      | RTOS         |      | idle loop /  |      | host call /  |
| init         +----->+ scheduler    +----->+ display      +----->+ session      |
|              |      | entry        |      | reached      |      | attempted    |
+--------------+      +--------------+      +--------------+      +------+-------+
                                                                          |
                                                                          v
                                                          +-------------------------+
                                                          | table / config storage  |
                                                          | accessed                |
                                                          +-------------------------+
```

This corresponds 1:1 with the M0–M12 boot milestone ladder in [`boot-milestones.md`](boot-milestones.md).

## Process / transport boundary

```
+----------------+        TCP/WS/pipe/serial         +-----------------+
| coinline-emu   | <--------------------------------> | coinline (MIT)  |
| (GPL-isolated) |     (raw modem UART bytes)         |  (Host/Server)  |
+----------------+                                    +-----------------+

Boundary contract:
  - Bytes in the pipe correspond exactly to bytes the firmware would have
    sent through the modem on real hardware.
  - The bridge does not synthesize any application-layer behavior.
  - Modem control signals (DCD/CTS/RTS/DTR) are encoded in optional
    side-band frames; defaults assume permanently-asserted DCD/CTS in lab
    setups.
  - There is no shared library, header, or compiled object between the
    two processes. Each side implements the wire protocol independently.
```

See [`modem-uart-host-bridge.md`](modem-uart-host-bridge.md) for the wire-level details.

## Component responsibilities

### Z180 CPU + MMU + internal peripherals

Provides instruction execution and the on-chip peripherals (ASCI 0/1, PRT 0/1, INT controller, DMA, refresh, wait-state controller). MAME's Z180 device covers these directly; the MIT-clean fallback track wires a permissive Z80 core to a custom peripheral block. Detail in [`z180-core.md`](z180-core.md) and [`z180-internal-peripherals.md`](z180-internal-peripherals.md).

### Memory map

Decodes addresses to ROM, RAM, NVRAM, banked regions, table storage, firmware-download staging, vectors, stack, and heap. Two regions of particular note:

- **NVRAM** — battery-backed; persists across runs via `fixtures/nvram/*.json` images.
- **Table storage** — receives downloaded tables from CoinLine Host; persistence rules in [`nvram-and-table-storage.md`](nvram-and-table-storage.md) and [`table-download-behavior.md`](table-download-behavior.md).

### I/O map

Decodes I/O port reads/writes. Unknown-port reads/writes are **logged**, not silenced — see [`io-port-map.md`](io-port-map.md) → *Unknown-port policy*.

### Devices

Each device follows the contract in [`device-model.md`](device-model.md): reset, I/O read/write, tick, interrupt generation, state snapshot, trace logging, fixture input, deterministic tests. Per-device docs:

| Device | Doc | Spec |
| ------ | --- | ---- |
| VFD | [`vfd-emulation.md`](vfd-emulation.md) | [`../specs/vfd-device.spec.md`](../specs/vfd-device.spec.md) |
| Keypad | [`keypad-emulation.md`](keypad-emulation.md) | [`../specs/keypad-device.spec.md`](../specs/keypad-device.spec.md) |
| Hookswitch + handset | [`hookswitch-and-handset.md`](hookswitch-and-handset.md) | (within keypad and audio specs) |
| Card reader | [`card-reader-emulation.md`](card-reader-emulation.md) | [`../specs/card-reader-device.spec.md`](../specs/card-reader-device.spec.md) |
| Smart card | [`smartcard-emulation.md`](smartcard-emulation.md) | [`../specs/smartcard-device.spec.md`](../specs/smartcard-device.spec.md) |
| Coin validator | [`coin-validator-emulation.md`](coin-validator-emulation.md) | [`../specs/coin-validator-device.spec.md`](../specs/coin-validator-device.spec.md) |
| Modem UART + host bridge | [`modem-uart-host-bridge.md`](modem-uart-host-bridge.md) | [`../specs/modem-uart-device.spec.md`](../specs/modem-uart-device.spec.md) + [`../specs/host-bridge.spec.md`](../specs/host-bridge.spec.md) |
| NVRAM + table storage | [`nvram-and-table-storage.md`](nvram-and-table-storage.md) | [`../specs/nvram-storage-device.spec.md`](../specs/nvram-storage-device.spec.md) |
| Alerter audio | [`alerter-audio.md`](alerter-audio.md) | [`../specs/alerter-audio-device.spec.md`](../specs/alerter-audio-device.spec.md) |
| Lock/door/vault/service | [`lock-door-vault-service.md`](lock-door-vault-service.md) | [`../specs/lock-door-service-device.spec.md`](../specs/lock-door-service-device.spec.md) |

## Determinism and observability

All scenario runs are deterministic given a fixed firmware binary, board profile, NVRAM image, scenario JSON, and host-bridge transcript. Determinism is achieved by:

- A single, well-defined cycle source (the Z180 cycle counter).
- Fixed seeds for any device that uses pseudo-randomness (e.g., handset background noise — disabled in scenarios).
- Snapshotted host-bridge transcripts replayed verbatim during regression.

Observability is provided by:

- Boot trace (PC samples at boot milestones).
- I/O trace (per-port reads/writes with PC).
- UART transcript (TX/RX bytes with timestamps).
- VFD buffer snapshots (text + raw command stream).
- NVRAM diff (writes since reset).
- Host-bridge transcript (frames with timestamps).

These artifacts are bundled per [`evidence-bundles.md`](evidence-bundles.md).

## Threading and timing model

The MAME engine runs the machine on a single emulation thread; UI and host-bridge I/O are threaded separately and synchronized via MAME's existing scheduler. The MIT-clean track adopts the same model. Scenario runner steps are advanced in cycle units, not wall-clock units, to keep tests deterministic.

## Failure isolation

If a device misbehaves (e.g., asserts an unsupported interrupt), the failure is contained to that device's trace and does not corrupt other devices' state. The machine driver records the offending PC, port, and value, and either continues with a logged unknown-port response or halts the CPU based on the board profile's `on_unknown_port` policy.
