# Hardware spec: uPD7759 voice ROM execution path (chip-faithful model)

**Hardware IDs:** **HW-VOIC-003** (execution fidelity), extends **HW-VOIC-001** / **HW-VOIC-002** (umbrella: [`HW-VOICE-ALERT-family.spec.md`](HW-VOICE-ALERT-family.spec.md))

## Purpose

Define **silicon-oriented** behavior for the terminal’s **NEC uPD7759-class** voice playback path so the emulator can:

1. **Execute the same ROM bitstream** the physical chip consumes (no parallel software ADPCM path as the source of truth).
2. Expose **busy / complete** to the host CPU using **`busy_r()`** (or equivalent) from the **MAME `upd7759_device`** core.
3. Preserve **bitwise fidelity** of shipped voice ROM images for forensic hash comparison (no runtime mutation of dumps).
4. Optionally model **post-DAC reconstruction** (low-pass / level) before the switched handset audio path.

Companion integration spec: [`voiceware-upd7759-core-execution.spec.md`](../voiceware-upd7759-core-execution.spec.md).

## Scope

| In scope | Out of scope |
| -------- | -------------- |
| ROM banking, reset, start/stop sequencing visible to Z180 | RTOS task source code parity (host firmware remains black-box) |
| MAME uPD7759 as reference implementation of the silicon algorithm | Voiceware **directory / phrase table** layout unless needed to **feed** the chip |
| `0x40` / `0x42` / `0x61` CPU-facing semantics | EEPROM bit-bang on shared PIO B when CS asserted |
| INT0 (or profiled IRQ) alignment with playback completion | Full analog handset hybrid network |

## Firmware-facing interface (known today)

Per [`fixtures/board/voiceware-command-map.json`](../fixtures/board/voiceware-command-map.json):

| Port | Role |
| ---- | ---- |
| `0x40` | Hardware control; **bit `0x08`**: voice reset (active levels per board; emulator inverts to uPD `/RESET`) |
| `0x42` | Bank latch **low nibble** `0x0F` → uPD external ROM bank select |
| `0x61` | Phrase / command write; **status read** returns configurable idle / busy / fault bytes (defaults `0xFF` / `0x7F` / `0x7F`) via `COINLINE_VOICEWARE_0x61_*` until electrical truth table is known (see *Open items*) |

ROM container: **`voicew`** region — 2 MiB, U16 lower 1 MiB + U26 upper 1 MiB, 16 × 128 KiB banks (`millennium.cpp`).

## Normative execution model

1. **Single source of PCM:** `upd7759_device` (MAME) advances internal state from **ROM bytes** in the selected bank.
2. **Phrase selection:** If the board’s firmware does **not** write raw uPD command bytes at `0x61` but instead a **phrase index**, a thin **adapter** (still in emulator code) may translate index → **start address / trigger** **only** using data structures that exist in ROM **without modifying** the ROM file. The **chip** must still be the block that decodes ADPCM.
3. **Busy:** Host-visible “playing” derives from **`busy_r()`** (idle when busy line false per MAME convention — confirm against datasheet).
4. **Completion:** IRQ or polled status must **edge-align** with **busy deassert** (or datasheet-defined “end” condition), not a fixed 100 µs poll loop unless validated as sufficient.
5. **Retrigger:** Emulator policy is **`COINLINE_VOICEWARE_RETRIGGER_POLICY`** — `suppress` (default: ignore duplicate phrase+bank strobes while playing) or `allow` (always issue uPD `port_w` + start). Datasheet may mandate a third mode later.

## Post-chip analog (informative, phase 2)

- First-order or documented **reconstruction filter** after the notional DAC.
- **Gain staging** to match alerter / routing device headroom before `millennium_audio_route_device` handset path.
- Parameters become **fixtures** (`fixtures/audio/voiceware-analog-profile.json` — to be added in implementation tranche) once captures exist.

### Emulator mapping (MAME)

- **Tap:** `upd7759_device` → `FILTER_RC` (`vw_upd_filter`) → `vwspk` (same speaker node as the legacy voiceware PCM stream).
- **Default low-pass:** single-pole RC, `R = 10 kΩ`, `C = 4.7 nF` (~3.4 kHz) via `machine/rescap.h` helpers (`RES_K`, `CAP_N`).
- **Bypass:** `COINLINE_VOICEWARE_ANALOG_BYPASS=1` sets `C = 0` (MAME treats this as filter disabled for `FLT_RC_LOWPASS`).
- **Gain:** `COINLINE_VOICEWARE_ANALOG_GAIN` scales the filtered tap into `vwspk` (clamped in code to `(0, 4]`; default `1`).

## Resolved from NEC IC-2323A (µPD7759)

See repo PDF `UPD7759.pdf` and text extract `UPD7759-datasheet-extract.md`.

- **RESET:** Active **low**; **t_RST ≥ 18.5 µs** (standalone); also **≥ 12** oscillator cycles in operation/standby rules.
- **BUSY:** **Active-low** while synthesizing; **high-Z** in standby; **t_SBO** / **t_SSO** / **t_BD** per AC table.
- **Start:** **ST↓** while **CS** low; **I0–I7** latched on **ST↑** (positive logic). **MD, ST, WR invalid while BUSY is low** ⇒ **no valid retrigger** during **BUSY low** (matches default emulator **suppress**).
- **ROM at chip:** **17-bit** byte address (**A0–A8** + **ASD0–7** address phase), **8-bit** data, **128 KiB** span (1 Mbit) in standalone; **slave** mode uses host **WR** path instead of external ROM fetch.
- **IRQ:** Chip has **no interrupt pin**; terminal **INT0** is **glue** from **BUSY** (or equivalent).

## Open items (bench / board only)

Document owner: hardware validation; emulator consumes answers as constants.

1. **`0x61` read (Z180):** The µPD7759 does **not** define this port—levels are **board buffers / pulls / inversion**. Capture **idle** / **busy** / **fault** with LA; set `COINLINE_VOICEWARE_0x61_*` and record `phrase_0x61_*` in `COINLINE_VOICEWARE_STARTUP_JSON`.
2. **Board start semantics:** How phrase **write** on `0x61` maps to **ST** / **CS** / latch timing vs **t_DW**, **t_CC**, **t_RS** (reference firmware ordering in `terminal_18_upd7759_voiceware_banked_emulator_spec.yaml`).
3. **ROM images:** Confirm MAME **ROM header** / directory layout vs **physical** dumps without `machine_start` patches when core flag is on.

## Trace / evidence

| Artifact | Content |
| -------- | ------- |
| `voiceware-trace.jsonl` | Per existing schema; add fields: `upd7759_busy`, `rom_bank`, `execution_path: "upd7759_core"` |
| Optional WAV | Post-filter tap for A/B vs reference hardware |

## Acceptance (hardware-facing)

- [ ] No **write** to `memregion("voicew")` at runtime for bring-up (remove or gate `patch_voicew_rom_headers_for_upd7759_master` when core path enabled).
- [ ] **SHA-256** of `voice_a.bin` / `voice_b.bin` matches reference manifests in CI when core path enabled.
- [ ] Busy/IRQ timing within **datasheet-bounded** tolerance vs golden trace (once captures exist).

## Related

- [`../voiceware-upd7759-core-execution.spec.md`](../voiceware-upd7759-core-execution.spec.md)
- [`../../test-plans/hardware/HW-UPD7759-VOICE-parity-tests.md`](../../test-plans/hardware/HW-UPD7759-VOICE-parity-tests.md)
