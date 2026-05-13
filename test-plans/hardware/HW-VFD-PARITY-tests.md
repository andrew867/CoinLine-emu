# Test plan: VFD reference program revision reference parity (HW-VFD-003 / HW-VFD-004)

| Field | Value |
| ----- | ----- |
| **Spec** | [`../../specs/hardware/HW-VFD-reference program revision-PARITY.spec.md`](../../specs/hardware/HW-VFD-reference program revision-PARITY.spec.md) |
| **Status** | Active — enterprise gate for “perfect” VFD track |
| **Owner** | Emulator + hardware validation (shared) |

## Objectives

1. Prove **I/O decode parity**: **A0** and **CS** from PIO B gate `0x60` traffic identically to reference low-level routines.
2. Prove **2×20 semantics**: DC1–DC7, clear, cursor command path, and status busy windows match the normative spec.
3. Prove **font parity** where automated bitmap comparison is feasible.
4. Stage **11-line** (HW-VFD-004) tests without blocking HW-VFD-003 CI closure.

## Test classification

| Class | Meaning | Automation |
| ----- | ------- | ---------- |
| **A** | Pure unit / synthetic stream — no full firmware | Required in CI |
| **B** | Emulator integration — firmware boot to milestone | Required nightly or on-demand |
| **C** | Bench / LA — golden hardware capture | Optional until artifacts exist |

## Preconditions

- Board profile exposes `display.busy_cycles_*` and `display.variant` (`fixtures/board/board-profile-*-vfd.json`).
- PIO B shadow readable in test harness (or mock) for Class A injection of **VFDA0** / **VFD_CSB** per write.
- Reference glyph table path: `reference firmware tree/VFD driver routine` (`the reference glyph table`) for bitmap extract (scripted diff).

## Class A — synthetic I/O (required)

| TC ID | Requirement | Procedure | Pass | Fail |
| ----- | ----------- | --------- | ---- | ---- |
| **TP-VFD-003-001** | CS inactive ignores `0x60` | Latch PIO B with CS high; write data; buffer unchanged | No cell change | Any mutation |
| **TP-VFD-003-002** | A0=1 cursor command | CS active, A0=1, write `25`; A0=0, write `'A'` | Cell index 25 is `'A'` | Wrong index |
| **TP-VFD-003-003** | A0=0 data DC1 normal | Write `0x11`; stream digits | Incremental fill left-to-right | Wrap/cursor wrong |
| **TP-VFD-003-004** | DC3 / DC4 cursor visibility | `0x13` then render; `0x14` | Cursor flag / overlay matches | Inverted or no-op |
| **TP-VFD-003-005** | DC5 horizontal scroll | Fill line; `0x15`; write at EOL | Scroll image matches reference algorithm | Static or corrupt |
| **TP-VFD-003-006** | DC6 blink + parameter | `0x16`, position byte | Blink region addressed | Missing 2nd byte handling |
| **TP-VFD-003-007** | DC7 cancel blink | After DC6, `0x17` | Blink canceled | Stuck blink |
| **TP-VFD-003-008** | Clear `0x0C` | Fill buffer; `0x0C` | **All** cells space; busy uses clear budget | Home-only or partial clear |
| **TP-VFD-003-009** | Busy timing | Per-op writes with `f_cpu` from profile | `status_read` matches predicted cycles ±1 tick | Drift or wrong class |
| **TP-VFD-003-010** | `0x1A` luminance path | Documented byte sequence from open item | Luminance state or stub accepted | Confused with decoration-clear |
| **TP-VFD-003-011** | Escape annex | Boot-captured `ESC` sequences | Consumes payload; no stray glyphs | Regression vs `vfd-driver-source-map` |

> **Note:** Until implementation lands, mark failing TCs as **expected fail** in a dedicated `ctest` label `vfd-parity` to avoid breaking mainline; remove label once green.

## Class A — font parity (required where feasible)

| TC ID | Procedure | Pass |
| ----- | --------- | ---- |
| **TP-VFD-003-020** | Extract 5-byte columns for ASCII `0x20–0x7E` from reference ASM table; compare to `millennium_vfd_gfxfont` | Identical |
| **TP-VFD-003-021** | Extended region `0x80–0xFF` per reference table | Identical or documented delta list |

## Class B — firmware-in-the-loop

| TC ID | Procedure | Evidence |
| ----- | --------- | -------- |
| **TP-VFD-003-100** | Boot to **M6** then **M10** on 2-line profile | `boot-trace.jsonl`, `vfd-final-text.txt` |
| **TP-VFD-003-101** | Compare idle snapshot to `fixtures/display/vfd-2line-idle.json` | Byte-for-byte text rows |
| **TP-VFD-004-100** | 11-line profile: softkey row + ad region | `vfd-11line-ad` fixture + PNG |

**Failure criteria:** Milestone missing; text fixture mismatch; unexplained `unknown_escape` when `COINLINE_VFD_WARN_UNKNOWN_ESCAPE=1`.

## Class C — bench (optional)

| TC ID | Input | Golden | Status |
| ----- | ----- | ------ | ------ |
| **TP-VFD-003-C01** | LA on `0x60`, A0, CS | Timestamped CSV | `SKIP` until capture |
| **TP-VFD-003-C02** | Status read bit map | JSON truth table | `SKIP` until capture |

## Fixtures and artifacts

| Asset | Role |
| ----- | ---- |
| `fixtures/display/vfd-2line-idle.json` | Idle equivalence |
| `fixtures/board/board-profile-2line-vfd.json` | Timing + variant |
| `build/runs/<run>/vfd-trace.jsonl` | Implementation debug |
| `build/runs/<run>/evidence-summary.md` | Gate bundle |

## CI mapping

| Target | Scope |
| ------ | ----- |
| `ctest -R test_vfd_` | Baseline regression (existing) |
| `ctest -L vfd-parity` | New HW-VFD-003 suite (add label in CMake) |

## Implementation sequencing (tranches)

1. **Tranche 1 (HW-VFD-003 core):** PIO-demux + command cursor + DC table + `0x0C` clear + busy classes + update unit tests.
2. **Tranche 2 (font):** TP-VFD-003-020/021 automation.
3. **Tranche 3 (HW-VFD-004):** 11-line model + TP-VFD-004-100.

## Sign-off

| Role | Action |
| ---- | ------ |
| Engineering | All Class A green on `main` for Tranche 1 |
| HW / validation | Class C artifacts attached OR open items explicitly time-boxed |
| Product | Accept documented deltas (if any) in `` |

## Cross-references

- [`../vfd-tests.md`](../vfd-tests.md)
- [`HW-VFD-family-tests.md`](HW-VFD-family-tests.md)

