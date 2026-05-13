# Test plan: uPD7759 core execution & Voiceware parity

**Program:** Voiceware hardware-faithful execution (see [`docs/status/UPD7759-CORE-EXECUTION-PROGRAM.md`](../../docs/status/UPD7759-CORE-EXECUTION-PROGRAM.md))  
**Specs:** [`specs/hardware/HW-UPD7759-VOICE-ROM-EXECUTION.spec.md`](../../specs/hardware/HW-UPD7759-VOICE-ROM-EXECUTION.spec.md), [`specs/voiceware-upd7759-core-execution.spec.md`](../../specs/voiceware-upd7759-core-execution.spec.md)

## Test tiers

| Tier | Environment | Purpose |
| ---- | ----------- | ------- |
| **T0** | CMake unit / host | ROM hash, fixture shape, adapter pure functions |
| **T1** | MAME driver off-line | `busy_r()` correlation, bank switch, reset sequence |
| **T2** | Full machine + firmware | Boot milestones M5*, voice IRQ, trace JSONL |
| **T3** | Golden hardware (future) | WAV / timing capture compare |

### CI contract (GitHub Actions)

Workflow [`.github/workflows/coinline-emu-cpp-tests.yml`](../../../../.github/workflows/coinline-emu-cpp-tests.yml) sets **`COINLINE_VOICEWARE_UPD7759_CORE=1`** for the job environment (redundant with driver **default on**, but keeps the contract explicit) and runs **full** `ctest` on Ubuntu for `coinline-emu` (CMake). That covers **T0** items implemented as labeled tests (see T0 rows). **T1–T2** remain **MAME / evidence-bundle** tier unless a separate job is added with `EXTERNAL_MAME_ROOT` and ROM artifacts.

---

## T0 — Unit and static tests

| ID | Case | Pass criteria |
| -- | ---- | --------------- |
| T0.1 | ROM manifest SHA | `voice_a.bin` / `voice_b.bin` match pinned hashes in fixture |
| T0.2 | No runtime ROM patch | With core flag on, `memregion("voicew")` bytes at bank headers unchanged after `machine_start` |
| T0.3 | Phrase directory lookup | Adapter-only function: phrase index + bank → start offset matches golden table for sample phrases (from `tools/voiceware` or captured JSON) |
| T0.4 | voiceware-command-map | Existing [`test_voiceware_fixture_replay.cpp`](../../tests/devices/test_voiceware_fixture_replay.cpp) still passes |
| T0.5 | `0x61` byte parse + env overlay | [`test_voiceware_phrase_port.cpp`](../../tests/devices/test_voiceware_phrase_port.cpp): hex parse + `coinline_voiceware_phrase_port_levels_apply_hex_env` (idle/busy/fault defaults and overrides) |

## T1 — Device / integration (no full UI)

| ID | Case | Pass criteria |
| -- | ---- | --------------- |
| T1.1 | Reset pulse | After `0x40` assert, `busy_r()` idle; after release + bank, chip accepts start |
| T1.2 | Bank switch | `set_rom_bank` invoked on `0x42` commit; ROM read address matches expected 128 KiB window |
| T1.3 | Busy lifecycle | From phrase start to completion, `busy_r()` follows monotonic pattern expected by MAME (document actual sequence in test comment) |
| T1.4 | INT0 edge | Simulated poll: IRQ fires within **N** host cycles of busy deassert; **N** is bounded by the voiceware poll interval / stream quantum used in the driver (tighten when a dedicated busy-edge harness exists — not an arbitrary large slack) |

## T2 — Full stack (CoinLine driver)

| ID | Case | Pass criteria |
| -- | ---- | --------------- |
| T2.1 | Boot phrase | First boot phrase (e.g. `0xB3`) produces non-silent WAV when trace requests capture; `execution_path` = `upd7759_core` in trace |
| T2.2 | M5 ordering | M5V / M5A / M5C milestones unchanged vs baseline trace (regression JSON) |
| T2.3 | Voice IRQ | `voice_irq0_assert` in voiceware trace aligns with busy deassert within tolerance |
| T2.4 | PIO B alias | Strobed phrase path still works or explicitly documented as legacy-only |

## T3 — Hardware golden (blocked on lab)

| ID | Case | Pass criteria |
| -- | ---- | --------------- |
| T3.1 | WAV correlation | Normalized cross-correlation ≥ threshold vs capture for fixture phrases |
| T3.2 | `0x61` levels | Idle / busy / **fault** (reset-active) bytes match logic analyzer; until then CI uses defaults and optional `COINLINE_VOICEWARE_0x61_IDLE` / `_BUSY` / `_FAULT` to mirror bench captures (recorded in `COINLINE_VOICEWARE_STARTUP_JSON` as `phrase_0x61_*`) |

---

## Regression strategy

- Maintain **dual-path** tests while migrating: same phrase list run under **legacy** and **core** flags; compare duration ± tolerance and spectral envelope (not bit-exact PCM unless clock-locked).

## Exit gate (program)

All **T0** + **T1** + selected **T2** green; **T3** optional for “hardware certified” label.
