# Program: uPD7759 core execution for Voiceware (status)

**Owner:** emulator / audio  
**Status:** **Implemented (partial)** — core `upd7759_device` path, `busy_r()`-grounded **playing()** / **0x61** / INT0 polling, optional **FILTER_RC** + gain, legacy fallback env, CMake T0 fixtures, CI. **`COINLINE_VOICEWARE_UPD7759_CORE` defaults to on** (set `0`/`off`/`false`/`no` to disable). **Chip** timing / ST–BUSY / reset / retrigger rules are **grounded in NEC IC-2323A** (see extract below). **Z180 `0x61` read levels** are still **board-specific**; keep env overrides (`COINLINE_VOICEWARE_0x61_*`) until LA on the terminal net. Default **`suppress`** retrigger matches datasheet: **ST is invalid while BUSY is low**.

**Hygiene:** Follow [`AGENTS.md`](../../AGENTS.md); no proprietary filenames in public traces.

**Flags (env / INI)**

| Variable                                            | Default                  | Meaning                                                                                                                             |
| --------------------------------------------------- | ------------------------ | ----------------------------------------------------------------------------------------------------------------------------------- |
| `COINLINE_VOICEWARE_UPD7759_CORE` | **on** (default) | Core path: skip ROM header patch; use `upd7759_device`. Set `0` / `off` / `false` / `no` to restore legacy patch + software decode primary path. |
| `COINLINE_VOICEWARE_LEGACY_FALLBACK`                | off                      | When core is on and phrase lookup fails, allow software decoder.                                                                    |
| `COINLINE_VOICEWARE_ANALOG_BYPASS`                  | off                      | Disable `FILTER_RC` between uPD tap and `vwspk`.                                                                                    |
| `COINLINE_VOICEWARE_ANALOG_GAIN`                    | `1`                      | Route gain (0–4).                                                                                                                   |
| `COINLINE_VOICEWARE_0x61_IDLE` / `_BUSY` / `_FAULT` | `0xFF` / `0x7F` / `0x7F` | Hex byte overrides for Z180 read of phrase/status port (fault while voice reset asserted).                                          |
| `COINLINE_VOICEWARE_RETRIGGER_POLICY`               | `suppress`               | `suppress` = ignore duplicate phrase+bank `0x61` write while playing; `allow` / `always` / `always_strobe` = always run start path. |

## Objective

Match the **PCD3349A** approach: **real device core** (here: MAME **`upd7759_device`**) consuming **the same ROM bytes** as hardware, with **busy/IRQ** derived from **`busy_r()`**, **no runtime ROM patching** when the core path is enabled, and an optional **reconstruction filter** before the handset audio route.

## Documents

| Doc                      | Path                                                                                                                                                                             |
| ------------------------ | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Hardware spec            | `[specs/hardware/HW-UPD7759-VOICE-ROM-EXECUTION.spec.md](../../specs/hardware/HW-UPD7759-VOICE-ROM-EXECUTION.spec.md)`                                                           |
| Integration spec         | `[specs/voiceware-upd7759-core-execution.spec.md](../../specs/voiceware-upd7759-core-execution.spec.md)`                                                                         |
| Test plan                | `[test-plans/hardware/HW-UPD7759-VOICE-parity-tests.md](../../test-plans/hardware/HW-UPD7759-VOICE-parity-tests.md)`                                                             |

## Datasheet Q&A (NEC IC-2323A, µPD7759 — repo PDF)

Full citations: `` and text summary ``.

1. **Digital I/O (standalone / master):** **RESET** = active **low** (≥12 osc cycles low; **t_RST ≥ 18.5 µs** min). **BUSY** = **active-low output** while synthesizing; **high-Z** in standby. **ST** + **CS** = start (**ST↓** while **CS** low); **I0–I7** = message code, **positive logic**, latched on **ST↑**. **MD** high = standalone. **Rule:** **MD, ST, and WR are invalid while BUSY is low.**
2. **“Port read” / bus type:** The IC does **not** define a CPU **readback** on I0–I7 (they are **inputs**). **ASD0–7** are **time-multiplexed** ROM address out / 8-bit ROM data in. DC tables show **CMOS-style** input thresholds (**V_IH ≈ 0.7 V_DD**, **V_IL ≤ 0.3 V_DD** at 5 V) and **push-pull-like** output levels on BUSY/address; **no open-drain** wording for BUSY. **Z180 `0x61`** levels ⇒ **measure on terminal glue**, not from µPD7759 alone.
3. **Timing (standalone highlights):** **t_RST** ≥ **18.5 µs**; **t_RS** (ST after RESET↑) ≥ **200 µs** (operation) or **1.6 ms** (standby); **t_SBO** (BUSY after ST↓) **6.25–10 µs**; **t_SSO** (speech after BUSY↓) **~2.1–2.2 ms**; **t_BD** ≥ **15 µs**. **No IRQ pin** on chip—host **INT0** is **board-derived from BUSY** (budget **t_SSO** + host sampling).
4. **Retrigger:** **ST (and WR) invalid while BUSY is low** ⇒ **no valid new start** during active **BUSY low**; matches default **`COINLINE_VOICEWARE_RETRIGGER_POLICY=suppress`**. **`allow`** remains a **bench override**, not datasheet-guaranteed.
5. **ROM / MAME:** External **1 Mbit** ROM; **A0–A8** + **ASD0–7** address ⇒ **17 bits** = **128 KiB** byte space; **8-bit** ROM data on ASD bus. **Slave mode** uses **WR**/host data path, not external ROM fetch. **128 KiB bank** presentation to `upd7759_device` matches chip span; PCB bank mux is documented in the voiceware banked emulator spec under `specs/`.

## Still bench-only (not in IC-2323A as Z180 levels)

- **Measured `0x61` idle/busy/fault** bytes on the **terminal** data bus (buffers, inversions, pull-ups).
- **T3** golden WAV / correlation vs hardware captures.

## Deferred

- **T3** golden WAV tests until hardware captures exist.
- Optional: tighten emulator **timing** to **t_RST** / **t_RS** / **t_SBO** bounds (today: `busy_r()`-driven functional model).

## Changelog

| Date       | Note                                                                                                                                                                    |
| ---------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 2026-05-09 | Initial program doc; specs + test plan + tranche prompt added.                                                                                                          |
| 2026-05-09 | Delivery 1: core flag + forensic ROM SHA in `COINLINE_VOICEWARE_STARTUP_JSON` / logerror.                                                                               |
| 2026-05-09 | Deliveries 2–6: phrase lookup, core playback, trace fields, analog tap, docs + register list.                                                                           |
| 2026-05-08 | Configurable `0x61` levels + retrigger policy; `test_voiceware_phrase_port`; CI workflow with core flag; TP8048 quasi/MOVD read + trace harness fixes for full `ctest`. |
| 2026-05-09 | **Core path default on** (`COINLINE_VOICEWARE_UPD7759_CORE` falsey to disable); 93C66 microwire: fix `pio_keypad_w` previous Port-B latch for SK edge detect. |

