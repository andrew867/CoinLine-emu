# PCD3349A behavioral TP ROM — implementation status

Public summary of chip-oriented telephony processor (TP) emulation work.

## Goal

Drive CP-visible telephony behavior from an **executable MCS-48-class** image (`firmware/telephony_subprocessor.rom`) running under **`millennium_am8048_core`**, wrapped by **`millennium_pcd3349a`** / **`millennium_pcd3349a_contract`**, instead of ad‑hoc host shortcuts.

## Wiring expectation (real hardware mirror)

- **Front panel → TP:** keypad matrix and hookswitch samples are TP-side inputs; the CP receives **framed CSI/O bytes** from the TP (see protocol YAML under `docs/protocols/` for query/response semantics).
- **TP clock:** ~3.579545 MHz TP crystal model in traces (`tp_xtal_hz`).
- **Integration:** `millennium_state` delivers CP→TP bytes, steps the TP core against CP time, and drains TP→CP transmit queue into the CSI/O path.

## CP→TP query coverage (behavioral ROM)

Single-byte commands implemented in firmware include: **`QUERY_HOOK_SWITCH_STATE` (0x30)**, **`QUERY_TELEPHONY_STATUS` (0x31)**, **`NEXT_CALL_IDLE` (0x32)** — no frame — **`QUERY_PWR_INTERRUPT_STATUS` (0x33)** → **`0x7A`** or one-shot **`0x7C`** if **`FLAGS1` reserved bit** was armed — **`QUERY_VERSION_NUMBER` (0x34)** → **`0xC2`** framed payload (default ASCII **`0772470`** + pad + additive checksum), **`CLEAR_TELEPHONY_STATUS` / `CLEAR_DTMF_DIALING` / `CLEAR_ERROR_REPORT` / `CHANGE_DTMF_FEEDBACK`** — accepted with no immediate reply — **`QUERY_CO_LINE_STATUS` (0x37)** → **`0x80`**, **`QUERY_ERROR_REPORT` (0x38)** → **`0xC4`**, **`QUERY_KEY_MATRIX` (0x3A)** → **`0x88`/`0x8A`**, **`QUERY_HANDSET_CONTINUITY` (0x3B)** → **`0x8C`/`0x8E`** from **`F_LINE_OK`**, **`RELEASE`/`SEIZE` hook relay** — no reply — plus **`TELEPHONY_CONFIG`** **`0xC0`** long frame. Runtime tone/call-state bytes **`0x10`–`0x2F`**, **`0x40`–`0x46`** still route through **`HANDLE_RUNTIME_CONTROL`**.

## Hook visibility (“happy dance”)

CP-facing interchange uses distinct **transition** and **steady** hook opcodes (same numeric contract used across terminal telephony documentation). Behavioral firmware:

1. Emits the **transition** byte when debounced hook state changes.
2. After a short delay implemented via **timer interrupt countdown**, emits the **steady-state** confirmation byte so the CP sees transition-then-stable sequencing (matches observed craft/service entry expectations after a physical off-hook/on-hook cycle).

## Timer requirement (8048 core)

`EN TCNTI` alone does not advance the internal event counter; firmware must **`STRT T`** so timer interrupts fire. Deferred steady-hook transmission runs from the timer ISR tail; without a running timer, only the transition byte would appear until timer stepping is fixed.

## Build artifacts

| Artifact | Location |
| -------- | -------- |
| Pre-built ROM image | `firmware/telephony_subprocessor.rom` (4 KiB, ships with the repo) |
| Source assembler tree | Not bundled — the ROM is a behavioural model; rebuild requires a private assembler source tree and an MCS-48-capable cross-assembler |
| Toolchain | Any MCS-48-capable cross-assembler (e.g. ASL + `p2bin`); pre-built image is sufficient for normal builds |

Overlap warnings from binary packing tools may appear when multiple `ORG` regions map into the same ROM image; treat as a packaging hygiene item, not as runtime semantics.

## Validation harness notes

Craft-entry acceptance scripts compare VFD traces, readiness traces, and **`tp-cp-keypad-protocol-trace.jsonl`** consumption events. An empty keypad-protocol trace invalidates downstream gates even when other milestones progress—see harness scripts under `tools/mingw64/`.

## Related documents

| Topic | Document |
| ----- | -------- |
| Hardware inference (TP + PCP) | [TP-HARDWARE-INFERENCE-MAP.md](TP-HARDWARE-INFERENCE-MAP.md) |
| Tone intent vs modem | [TP-DTMF-TONE-SPEC.md](TP-DTMF-TONE-SPEC.md) |
| TP architecture | [TP-8048-ARCHITECTURE-SPEC.md](TP-8048-ARCHITECTURE-SPEC.md) |

Logical connectivity is summarized in public device docs ([keypad-emulation.md](../keypad-emulation.md), [hookswitch-and-handset.md](../hookswitch-and-handset.md), [audio-routing-emulation.md](../audio-routing-emulation.md)).
