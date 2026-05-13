# Hardware spec: VFD reference program revision reference parity (2-line + 11-line program)

| Field                   | Value                                                                                                                |
| ----------------------- | -------------------------------------------------------------------------------------------------------------------- |
| **Spec IDs**            | **HW-VFD-003** (2×20 + I/O fidelity), **HW-VFD-004** (11-line graphical subsystem)                                   |
| **Extends**             | `[../vfd-device.spec.md](../vfd-device.spec.md)`                                                                     |
| **Status**              | Normative — drives implementation and test case IDs                                                                  |
| **Companion test plan** | `[../../test-plans/hardware/HW-VFD-reference program revision-PARITY-tests.md](../../test-plans/hardware/HW-VFD-reference program revision-PARITY-tests.md)` |

## Purpose

Define **closed, testable** requirements so the CoinLine VFD model can reach **reference parity** with the terminal’s **documented** display stack:

1. **Electrical / decode fidelity:** One physical I/O address for write/read, with **register select** not implied by the data byte alone.
2. **2×20 control vocabulary:** Mode and control bytes match the **reference header constants** used by the classic cell API (DC1–DC7, clear, cursor programming).
3. **Timing:** Host-visible **busy** windows are **bounded** and **operation-specific**, consistent with documented software delays unless superseded by bench capture.
4. **11-line variant:** Programming model and memory behavior aligned with the graphical driver path (separate phase from HW-VFD-003 closure).

## Normative references (in repository)

| Reference                                                                                      | Use                                                                       |
| ---------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------- |
| `reference firmware tree/display API header`                                                                | Opcode names and delay tick counts (10 µs granularity) for the 2-line API |
| `reference firmware tree/I/O definitions header`                                                               | `DISPLAY_PORT`, `VFDA0`, `VFD_CSB` bit positions on PIO port B            |
| `reference firmware tree/VFD interface routine`                                                              | Data vs command write sequencing; status read side                        |
| `reference firmware tree/VFD interface routine`                                                               | High-level call pattern: **data writes** vs **command writes** for cursor |
| `reference firmware tree/VFD driver routine`                                                               | 11-line build: pixel base, tables, softkey-related behavior               |
| `reference firmware tree/VFD API header`                                                                 | Conditional API surface for 11-line vs 2-line                             |

## Definitions

- **DISPLAY_BYTE:** An 8-bit value driven on the Z180 external I/O window for the panel data path (board: `0x60` when VFD chip-select is active).
- **A0 (VFDA0):** Line from host GPIO; **low** = *data* register, **high** = *command / status* register for the same `DISPLAY_BYTE` address.
- **CS (VFD_CSB):** Chip select; emulation must ignore display traffic when CS is deasserted (external UART may share the decode).
- **Linear cursor index:** For 2×20, values `0 … 39` with `0` = top-left, `19` = top-right, `20` = bottom-left (see reference `TOP_LINE_1ST_CHAR` / `BOTTOM_LINE_1ST_CHAR` constants).

## Scope

| In scope (HW-VFD-003)                                                                                                | Out of scope (later or bench-only)                                               |
| -------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------- |
| Demux **data** vs **command** writes using **latched PIO B** image at the moment of each `0x60` cycle                | Analog phosphor / PWM luminance physics                                          |
| **Command write:** linear cursor address (and any additional command bytes defined by panel datasheet once captured) | Full **HD64180** `OUT0` cycle timing vs glue PAL (unless trace proves a gap)     |
| **Data write:** printable glyphs `0x20–0x7E` (and extended charset per font contract)                                | Certification of a specific third-party VFD module part number without datasheet |
| **Data write:** DC codes per table in §2-line control vocabulary                                                     |                                                                                  |
| **Data read (status):** at minimum **busy** semantics; extend to full bitfield when bench truth exists               |                                                                                  |
| Busy duration mapping from reference **10 µs** tick macros → `busy_cycles_`* profile (document conversion formula)   |                                                                                  |
| **Clear (`0x0C`):** full cell buffer erase + cursor to reference home                                                |                                                                                  |

| In scope (HW-VFD-004)                                                   | Out of scope                                            |
| ----------------------------------------------------------------------- | ------------------------------------------------------- |
| Memory-mapped / port behavior for 11-line pixel RAM as in reference ASM | Unrelated ADSI task UI unless it shares the same device |

## Firmware-facing interface (normative)

### I/O

| Resource   | Direction | Condition       | Semantics                                                                                                                         |
| ---------- | --------- | --------------- | --------------------------------------------------------------------------------------------------------------------------------- |
| `0x60`     | W         | A0=0, CS active | **Data** path: character or DC control per §2-line control vocabulary                                                             |
| `0x60`     | W         | A0=1, CS active | **Command** path: **set cursor address** (linear index for 2×20); additional command opcodes only if datasheet + trace prove them |
| `0x60`     | R         | A0=1, CS active | **Status**; bit `0x80` = busy while operation in flight (minimum); other bits TBD bench                                           |
| PIO port B | W/R       | —               | Emulator holds shadow; **VFDA0** and **VFD_CSB** must be consulted on every display access                                        |

### Critical section

Reference firmware masks interrupts around VFD sequences. Emulation **need not** model interrupt latency inside the device; timing requirements are expressed via **busy** windows.

## 2-line control vocabulary (normative DC table)

Values are **data-path** bytes (`vfd_data_write` in reference C API) unless marked *command path only*.

| Byte   | Reference name (display API header)   | Required emulator behavior                                                                                                                                                        |
| ------ | ---------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `0x11` | NORMAL_MODE (DC1)            | Incremental write; exit horizontal/vertical scroll semantics per reference API                                                                                                    |
| `0x12` | VERTICAL_SCROLL_MODE (DC2)   | Vertical scroll / wrap behavior per reference `set_vfd_mode` pairing                                                                                                              |
| `0x13` | CURSOR_ON_MODE (DC3)         | Cursor **visible** state ON (rendering contract in test plan)                                                                                                                     |
| `0x14` | CURSOR_OFF_MODE (DC4)        | Cursor **visible** state OFF                                                                                                                                                      |
| `0x15` | HORIZONTAL_SCROLL_MODE (DC5) | Horizontal scroll behavior at EOL (reference semantics; may include host-side image scroll)                                                                                       |
| `0x16` | BLINKING_MODE (DC6)          | Requires **follow-on data byte** (blinking position code); composable with other modes per reference                                                                              |
| `0x17` | CANCEL_BLINKING_MODE (DC7)   | Cancel blink region / restore steady display per reference                                                                                                                        |
| `0x0C` | VFD_CLEAR_CODE               | **Full display clear** (all cells blanked per reference), then apply `busy_cycles_clear`; cursor position follows reference sequence (typically explicit command `0` after delay) |
| `0x1A` | VFD_SET_LEVEL                | **Luminance / intensity** command per reference (parameter rules TBD if second byte required — see open items)                                                                    |
| `0x1B` | DEFINE_VFD_CHAR (ESC)        | If treated as start of **escape sequence** in traces, subdecode per annex; must not collide with reference **single-byte** font-define usage without version flag                 |

**Command path (A0=1):** writing byte `N` sets DDRAM/cursor to linear address `N` (clamped to `0 … rows*columns-1`). No `ESC` prefix on this path.

## Timing (normative)

1. Each accepted **data** or **command** byte extends `busy_until` by an operation-specific interval.
2. Reference delays in `display API header` use **10 µs** units (e.g. `VFD_DC1_CHAR_DELAY = 10` → 100 µs). Emulator shall document:
  `busy_cycles_op = ceil(delay_10us * 10e-6 * f_cpu)` with `f_cpu` from board profile, **or** load constants from an explicit JSON map.
3. **Clear** uses the **longest** documented data-path delay (`VFD_CLEAR_DELAY`) unless bench shows otherwise.

## Status register (normative minimum)

- **Busy:** `bit7 = 1` while `cycle < busy_until`.
- **Ready:** `bit7 = 0` when idle.
- Additional bits: **reserved** until captured; test plan marks TCs as `SKIP` until bench JSON lands.

## Font and downloadable characters (HW-VFD-003 annex)

1. **ROM glyph bitmaps** for code points used on shipped builds shall match reference `the reference glyph table` **bit-for-bit** for the 2-line character set contract (automated diff in CI).
2. **User-defined glyphs** (`DEFINE_VFD_CHAR` / escape-download paths): storage size, index range, and persistence across clear/reset per datasheet + trace.

## 11-line subsystem (HW-VFD-004 summary)

Separate deliverable: model **pixel RAM**, **base address**, **cursor / active line / softkey label** contracts per `VFD driver routine` / `VFD API header` (conditional build). Must not break HW-VFD-003 profiles when `display.variant` selects 2-line.

## Traceability / evidence

| Artifact               | Content                                                        |
| ---------------------- | -------------------------------------------------------------- |
| `vfd-trace.jsonl`      | Per-byte: `a0`, `cs`, `dir`, `value`, decoded op, `busy_after` |
| `vfd-final-state.json` | Text plane + optional dot plane snapshot                       |
| Unit test stdout       | TC IDs from test plan                                          |

## Acceptance criteria (release gate)

- **HW-VFD-003:** `0x60` write path uses **PIO B** shadow to select data vs command decode.
- **HW-VFD-003:** DC table in this spec matches `millennium_vfd_model` (or profile flag documents intentional divergence — default **no divergence**).
- **HW-VFD-003:** `0x0C` clears full buffer; busy class matches clear timing.
- **HW-VFD-003:** Command-path cursor write places next **data** glyph at correct linear index.
- All **Class A** tests in companion test plan pass in CI.
- **HW-VFD-004:** Tranche 2 — Class A pixel/softkey TCs pass or are explicitly `SKIP` with owner.

## Open items (bench / datasheet)

1. Full **status** bit map on read (`0x60`, A0=1).
2. Electrical truth for **luminance** (`0x1A`): single byte vs multi-byte.
3. **Panel** datasheet command set if it **supersedes** `display API header` for any opcode (document delta in internal map).

## Related

- `[../vfd-device.spec.md](../vfd-device.spec.md)`
- `[../../docs/vfd-emulation.md](../../docs/vfd-emulation.md)`

