# Implementation roadmap

This document gives the high-level path from a docs-only project (today) to a deployable emulator that boots real terminal firmware and exercises CoinLine Host end-to-end. The detailed per-tranche breakdown is in [`tranche-plan.md`](tranche-plan.md).

## Roadmap at a glance

```
E0 -> E1 -> E2 -> E3 -> E4 -> E5 -> E6 -> E7 -> E8 -> E9 -> E10 -> E11 -> E12 -> E13
 |     |     |     |     |     |     |     |     |     |     |      |      |      |
 |     |     |     |     |     |     |     |     |     |     |      |      |      +-- debug + CI hardening
 |     |     |     |     |     |     |     |     |     |     |      |      +-- CoinLine Host integration
 |     |     |     |     |     |     |     |     |     |     |      +-- scenarios + evidence bundles
 |     |     |     |     |     |     |     |     |     |     +-- boot-to-idle / service flows
 |     |     |     |     |     |     |     |     |     +-- coin + alerter
 |     |     |     |     |     |     |     |     +-- card + smartcard
 |     |     |     |     |     |     |     +-- NVRAM + tables
 |     |     |     |     |     |     +-- modem UART + host bridge
 |     |     |     |     |     +-- keypad + front-panel inputs
 |     |     |     |     +-- VFD + artwork
 |     |     |     +-- Z180 internal peripherals
 |     |     +-- MAME machine skeleton
 |     +-- firmware-evidence inventory (internal)
 +-- engine + license isolation
```

## Tranches

| Tranche | Theme | Goal |
| ------- | ----- | ---- |
| E0 | Engine + license | Pick engine, document boundary, scaffold project |
| E1 | Firmware evidence inventory | Inventory of firmware evidence and behavioural baselines |
| E2 | MAME machine skeleton | CPU + ROM + RAM + NVRAM + I/O traps + boot trace |
| E3 | Z180 internal peripherals | MMU, ASCI, PRT, INT, DMA, wait states |
| E4 | VFD + artwork | 2-line and 11-line VFD; MAME `.lay` overlay |
| E5 | Keypad + front-panel inputs | Keypad matrix, hook, quick-access, volume/language, lock/door/vault |
| E6 | Modem UART + host bridge | ASCI/UART, DCD/CTS/RTS/DTR, TCP/WS bridge to CoinLine Host |
| E7 | NVRAM + tables | EEPROM/NVRAM image, config/table storage, factory/corrupt images |
| E8 | Card + smart card | Magstripe timing; smart-card reset/ATR/APDU or memory-card |
| E9 | Coin + alerter + lock/door/vault | Coin pulses, denominations, errors; alerter audio; security inputs |
| E10 | Boot to idle | Real firmware boot through M0–M10 milestones, idle display, service entry |
| E11 | Scenarios + evidence | JSON scenarios, automated UAT, evidence bundles |
| E12 | CoinLine Host integration | Real firmware modem path to CoinLine Host; table/download/upload flows |
| E13 | Debugging + artwork + CI | Debug tools, artwork polish, CI hardening, regression suite |

## Dependencies

- E1 depends on access to `../firmware source tree`. Without that input, E1 is replaced by binary-only inventory (firmware binary inspection, modem capture analysis, hardware datasheets) and the public maps are still authored, marked **OPEN QUESTION** where evidence is missing.
- E4 depends on `docs/images/terminal-unbranded.webp` for the artwork base. Without that input, the emulator runs headless until a substitute is supplied.
- E12 depends on a deployable CoinLine Host instance.

## Acceptance gates

| Gate | Criterion |
| ---- | --------- |
| Skeleton boots | E2 complete; emulator launches, executes the reset vector, enters startup code (M1+M2). |
| First display | E4 complete; firmware writes to the VFD and the emulator captures the buffer (M6). |
| Keypad reaches firmware | E5 complete; firmware acknowledges keypress events (M7). |
| Idle reached | E10 complete; firmware reaches the idle loop or idle display (M10). |
| Host call attempted | E12 complete; firmware initiates a host call via the host bridge (M11). |
| Tables persisted | E12 complete; downloaded tables appear in NVRAM/table storage and survive reset (M12). |
| Scenarios green | E11 complete; scenario suite runs deterministically and produces evidence bundles. |

## Risk-driven sequencing

The roadmap is intentionally ordered to surface the highest-risk items first:

- **E0 license isolation** front-loads the GPL/MIT contamination risk.
- **E2 + E3 CPU + Z180 peripherals** front-loads the risk of an inadequate Z180 core.
- **E6 modem UART + host bridge** front-loads the risk of incorrect framing on the link to CoinLine Host.
- **E10 boot-to-idle** front-loads the risk that a missing peripheral causes a silent boot loop.

See [`risk-register.md`](risk-register.md) for the full risk catalogue.

## Audio / supervision track (A0–A7)

Orthogonal to E-tranches: implements **`millennium_voiceware_device`**, **`millennium_audio_route_device`**, **`millennium_supervision_device`**, extends **`millennium_audio_device`**, and wires traces/evidence. Does not replace E9 — **extends** it.

```
A0 (maps) -> A1 (voiceware) -> A2 (routing) -> A3 (alerter) -> A4 (supervision) -> A5 (integration) -> A6 (evidence) -> A7 (CI)
```

| Tranche | Doc |
| ------- | --- |
| A0–A7 | [`audio-implementation-roadmap.md`](audio-implementation-roadmap.md), [`tranche-plan.md`](tranche-plan.md) (Audio section), [`audio-mame-device-architecture.md`](audio-mame-device-architecture.md) |

**Gate:** Boot milestones **M6A–M11D** (see [`boot-milestones.md`](boot-milestones.md)) observable with firmware-driven traces.

## Reporting cadence

Each tranche's exit criterion is a populated row in [`compatibility-validation-items.md`](compatibility-validation-items.md) and a passing test in the relevant [`../test-plans/`](../test-plans/) plan.

