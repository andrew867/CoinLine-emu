# Tranche plan (E0–E13)

This document defines, for each implementation tranche, the goals, non-goals, files created, files modified, tests created, commands to run, acceptance criteria, blockers, and exit criteria.

The tranches are numbered E0–E13 (with audio sub-tranches A0–A7). The full per-tranche goals, files, tests, and exit criteria are documented in the sections below. **Audio extension (Voiceware uPD7759 core):** specs in [`specs/hardware/HW-UPD7759-VOICE-ROM-EXECUTION.spec.md`](../specs/hardware/HW-UPD7759-VOICE-ROM-EXECUTION.spec.md) drive a six-delivery rollout with one commit per delivery.

---

## Tranche E0 — Engine and license isolation

**Goals**

- Pick the engine (MAME-based or MIT-clean fallback).
- Document the license boundary in code, build, and CI.
- Scaffold the `coinline-emu/` source tree per [`project-structure.md`](project-structure.md) (still no functional code).

**Non-goals**

- No CPU bring-up.
- No firmware loading.

**Files created**

- `coinline-emu/.gitignore` (excludes firmware binary extensions by default).
- `coinline-emu/LICENSE` (GPL-2.0-or-later).
- `coinline-emu/CMakeLists.txt` and supporting build scripts under `coinline-emu/tools/`.
- `coinline-emu/.github/workflows/coinline-emu-cpp-tests.yml` (CI).

**Files modified**

- Repository-root `.gitignore` (if needed) to exclude firmware artifacts at the root.
- None in `coinline/`.

**Tests created**

**Commands to run**

```bash
```

**Acceptance criteria**

- CI workflow runs to green on a fresh clone.
- Build system stub exists and reports a clear "no source yet" message.
- Engine selection is recorded in [`engine-selection.md`](engine-selection.md).

**Blockers**

- None.

**Exit criteria**

- Engine choice locked.
- License boundary verifiable in CI.

---

## Tranche E1 — Firmware evidence inventory (internal)

**Goals**

- Inventory the firmware evidence available at `../firmware source tree`.
- Author the internal source-map docs: memory map, I/O ports, interrupts, Z180 registers, drivers, RTOS init.
- Derive `fixtures/board/memory-map.json`, `io-port-map.json`, `interrupt-map.json`, `device-map.json`.

**Non-goals**

- No public-facing claims yet beyond what is already in the public maps.
- No emulator code.

**Files created**

- `coinline-emu/fixtures/board/*.json` (planned).

**Files modified**

- `coinline-emu/docs/memory-map.md`, `io-port-map.md`, `interrupt-map.md` updated with newly known entries (and **OPEN QUESTION** rows for unknowns).

**Tests created**

- `coinline-emu/tests/fixtures/test_memory_map_schema.py`.
- `coinline-emu/tests/fixtures/test_io_port_map_schema.py`.
- `coinline-emu/tests/fixtures/test_interrupt_map_schema.py`.
- `coinline-emu/tests/fixtures/test_device_map_schema.py`.

**Commands to run**

```bash
pytest coinline-emu/tests/fixtures
```

**Acceptance criteria**

- All four `*-map.json` fixtures validate against their schemas.
- Internal source-map docs cite evidence files at `../firmware source tree` paths.
- Public maps reflect the same data minus internal references.

**Blockers**

- `../firmware source tree` not provided — fall back to binary-only inventory and mark public rows **OPEN QUESTION**.

**Exit criteria**

- Map fixtures committed; schemas locked.
- Public/internal wording boundary verified.

---

## Tranche E2 — MAME machine skeleton

**Goals**

- Implement `millennium.cpp/h`, `millennium_state.cpp/h`, `millennium_memory.cpp/h`, `millennium_io.cpp/h`.
- Wire Z180 CPU + ROM region (firmware loaded from `../firmware/flash.bin`) + RAM + NVRAM stub + I/O traps that log unknown ports per the policy in [`io-port-map.md`](io-port-map.md).
- Implement boot trace.

**Non-goals**

- No device behavior beyond logging.

**Files created**

- `coinline-emu/src/mame/coinline/millennium.cpp/h`.
- `coinline-emu/src/mame/coinline/millennium_state.cpp/h`.
- `coinline-emu/src/mame/coinline/millennium_memory.cpp/h`.
- `coinline-emu/src/mame/coinline/millennium_io.cpp/h`.
- `coinline-emu/tools/boot-trace-parser/` (basic).

**Files modified**

- MAME driver list in the external MAME tree (out-of-repo).

**Tests created**

- `coinline-emu/tests/boot/test_reset_vector.cpp`.
- `coinline-emu/tests/boot/test_first_instructions.cpp`.
- `coinline-emu/tests/devices/test_unknown_port_logging.cpp`.

**Commands to run**

```bash
ctest --test-dir build --label-regex "boot|unknown_port"
```

**Acceptance criteria**

- M0 and M1 reached deterministically.
- Unknown port reads/writes are logged with PC, port, R/W, value, cycle.

**Blockers**

- `../firmware/flash.bin` not provided.

**Exit criteria**

- Skeleton boots to startup code.

---

## Tranche E3 — Z180 internal peripherals

**Goals**

- Verify ASCI 0/1, PRT 0/1, INT controller, MMU, DMA, refresh, wait-state controller behavior under the firmware's actual usage pattern.
- Lock down register defaults and interrupt vector ordering.

**Non-goals**

- No external device behavior.

**Files created**

- `coinline-emu/tests/devices/test_z180_mmu.cpp`.
- `coinline-emu/tests/devices/test_z180_asci.cpp`.
- `coinline-emu/tests/devices/test_z180_prt.cpp`.
- `coinline-emu/tests/devices/test_z180_int.cpp`.
- `coinline-emu/tests/devices/test_z180_dma.cpp`.

**Files modified**

- `millennium_state.cpp` (clock and wait-state config).

**Tests created** — see above.

**Commands to run**

```bash
ctest --test-dir build --label-regex z180
```

**Acceptance criteria**

- M3 (RAM init) and M4 (Z180 registers initialized) reached.
- Z180 register dumps match expected values from internal source-map evidence (or are flagged **OPEN QUESTION**).

**Blockers**

- None beyond E2.

**Exit criteria**

- Z180 peripherals stable under firmware.

---

## Tranche E4 — VFD + artwork

**Goals**

- Implement `millennium_vfd.cpp/h` for both 2-line and 11-line variants.
- Place the front-panel image at `artwork/millennium-terminal-front.png` (sourced from `docs/images/terminal-unbranded.webp`).
- Author `artwork/millennium.lay` with VFD overlay regions per [`artwork-and-layout.md`](artwork-and-layout.md).

**Non-goals**

- No keypad input handling yet.

**Files created**

- `coinline-emu/src/mame/coinline/millennium_vfd.cpp/h`.
- `coinline-emu/artwork/millennium.lay`.
- `coinline-emu/artwork/millennium-terminal-front.png`.
- `coinline-emu/fixtures/display/vfd-2line-idle.json`.
- `coinline-emu/fixtures/display/vfd-11line-ad.json`.

**Tests created**

- `coinline-emu/tests/devices/test_vfd_command_decode.cpp`.
- `coinline-emu/tests/devices/test_vfd_buffer_snapshot.cpp`.

**Commands to run**

```bash
ctest --test-dir build --label-regex vfd
```

**Acceptance criteria**

- M6 (VFD write) reached; buffer snapshot exists.

**Blockers**

- `docs/images/terminal-unbranded.webp` (artwork) not provided.

**Exit criteria**

- Idle display visible on screen.

---

## Tranche E5 — Keypad + front-panel inputs

**Goals**

- Implement `millennium_keypad.cpp/h`, `millennium_security.cpp/h` (lock/door/vault/service inputs).
- Wire hookswitch and quick-access keys, volume, language.

**Non-goals**

- No card / coin / smart-card.

**Files created**

- `coinline-emu/src/mame/coinline/millennium_keypad.cpp/h`.
- `coinline-emu/src/mame/coinline/millennium_security.cpp/h`.

**Tests created**

- `coinline-emu/tests/devices/test_keypad_matrix.cpp`.
- `coinline-emu/tests/devices/test_hookswitch.cpp`.
- `coinline-emu/tests/devices/test_security_inputs.cpp`.

**Commands to run**

```bash
ctest --test-dir build --label-regex "keypad|hookswitch|security"
```

**Acceptance criteria**

- M7 (keypad scan) reached; firmware acknowledges keypress events.

**Blockers**

- None beyond E2–E4.

**Exit criteria**

- All front-panel inputs round-trip into the firmware.

---

## Tranche E6 — Modem UART + host bridge

**Goals**

- Implement `millennium_modem.cpp/h` and `millennium_hostbridge.cpp/h`.
- Bind ASCI/UART to a transport endpoint per [`modem-uart-host-bridge.md`](modem-uart-host-bridge.md).
- Implement modem state machine (idle, dialing, ringing, connected, busy, no answer, carrier lost, noisy line).

**Non-goals**

- No host-side application logic.

**Files created**

- `coinline-emu/src/mame/coinline/millennium_modem.cpp/h`.
- `coinline-emu/src/mame/coinline/millennium_hostbridge.cpp/h`.
- `coinline-emu/fixtures/modem/clean-connect.hex`.
- `coinline-emu/fixtures/modem/dropped-carrier.hex`.
- `coinline-emu/fixtures/modem/noisy-line.hex`.

**Tests created**

- `coinline-emu/tests/devices/test_modem_state_machine.cpp`.
- `coinline-emu/tests/devices/test_uart_transcript.cpp`.
- `coinline-emu/tests/integration/test_host_bridge_loopback.cpp`.

**Commands to run**

```bash
ctest --test-dir build --label-regex "modem|host_bridge"
```

**Acceptance criteria**

- M8 (UART/modem init) reached.
- Host-bridge loopback round-trips firmware bytes.

**Blockers**

- None beyond E2–E5.

**Exit criteria**

- Firmware can dial and reach a connected state with a loopback transport.

---

## Tranche E7 — NVRAM, tables, storage

**Goals**

- Implement `millennium_nvram.cpp/h`.
- Persist NVRAM image to disk between runs.
- Implement table storage region with checksum behavior.

**Files created**

- `coinline-emu/src/mame/coinline/millennium_nvram.cpp/h`.
- `coinline-emu/fixtures/nvram/factory-default.nvram.json`.
- `coinline-emu/fixtures/nvram/corrupt-checksum.nvram.json`.

**Tests created**

- `coinline-emu/tests/devices/test_nvram_persistence.cpp`.
- `coinline-emu/tests/devices/test_nvram_corrupt_checksum.cpp`.
- `coinline-emu/tests/devices/test_table_storage_region.cpp`.

**Commands to run**

```bash
ctest --test-dir build --label-regex "nvram|table_storage"
```

**Acceptance criteria**

- NVRAM image survives reset and produces consistent firmware boot state.
- Corrupt-checksum image triggers the firmware's recovery path (observed via VFD message).

**Blockers**

- None beyond E2–E6.

**Exit criteria**

- NVRAM round-trips deterministically.

---

## Tranche E8 — Card + smart card

**Goals**

- Implement `millennium_card.cpp/h` (magstripe with timing) and `millennium_smartcard.cpp/h` (smart-card or memory-card behavior).

**Files created**

- `coinline-emu/src/mame/coinline/millennium_card.cpp/h`.
- `coinline-emu/src/mame/coinline/millennium_smartcard.cpp/h`.
- `coinline-emu/fixtures/cards/magcard-valid.json`.
- `coinline-emu/fixtures/cards/magcard-invalid-lrc.json`.
- `coinline-emu/fixtures/cards/smartcard-valid.json`.
- `coinline-emu/fixtures/cards/smartcard-empty.json`.

**Tests created**

- `coinline-emu/tests/devices/test_card_swipe_timing.cpp`.
- `coinline-emu/tests/devices/test_card_lrc_validation.cpp`.
- `coinline-emu/tests/devices/test_smartcard_atr.cpp`.

**Commands to run**

```bash
ctest --test-dir build --label-regex "card|smartcard"
```

**Acceptance criteria**

- Valid magstripe swipe is accepted by firmware.
- Invalid LRC swipe is rejected with the firmware's expected error path.
- Smart-card ATR is honored.

**Blockers**

- None beyond E2–E7.

**Exit criteria**

- Card-call scenarios viable.

---

## Tranche E9 — Coin validator, alerter, lock/door/vault

**Goals**

- Implement `millennium_coin.cpp/h` (coin pulses and denominations) and `millennium_audio.cpp/h` (alerter audio).

**Files created**

- `coinline-emu/src/mame/coinline/millennium_coin.cpp/h`.
- `coinline-emu/src/mame/coinline/millennium_audio.cpp/h`.

**Tests created**

- `coinline-emu/tests/devices/test_coin_pulse_train.cpp`.
- `coinline-emu/tests/devices/test_coin_denominations.cpp`.
- `coinline-emu/tests/devices/test_alerter_audio.cpp`.

**Commands to run**

```bash
ctest --test-dir build --label-regex "coin|alerter"
```

**Acceptance criteria**

- Firmware accepts the full denomination set per board profile.
- Coin error paths trigger expected firmware messages.

**Blockers**

- None beyond E2–E8.

**Exit criteria**

- Coin-call scenarios viable.

---

## Tranche E10 — Boot to idle and service flows

**Goals**

- Drive the firmware through M0–M10 deterministically.
- Implement service-mode entry for installer flows.

**Files created**

- `coinline-emu/fixtures/scenarios/boot-to-idle.json`.
- `coinline-emu/fixtures/scenarios/service-mode.json`.

**Files modified**

- Multiple `millennium_*` files as bring-up surfaces remaining issues.

**Tests created**

- `coinline-emu/tests/integration/test_boot_to_idle.cpp`.
- `coinline-emu/tests/integration/test_service_mode_entry.cpp`.

**Commands to run**

```bash
ctest --test-dir build --label-regex "boot_to_idle|service_mode"
```

**Acceptance criteria**

- Firmware reaches the idle display in `boot-to-idle.json`.
- Service mode entry is reproducible in `service-mode.json`.

**Blockers**

- Any unresolved peripheral issues from earlier tranches.

**Exit criteria**

- Idle display reproducible across runs and platforms.

---

## Tranche E11 — Debug tools

**Goals**

- Build `tools/boot-trace-parser`, `tools/io-trace-analyzer`, and a symbol/map loader.

**Files created**

- `coinline-emu/tools/boot-trace-parser/`.
- `coinline-emu/tools/io-trace-analyzer/`.

**Tests created**

- Tool-specific tests (CLI smoke tests, parsing tests).

**Acceptance criteria**

- Tools accept evidence-bundle artifacts and produce human-readable reports.

**Exit criteria**

- Debug workflow documented in [`debugging-guide.md`](debugging-guide.md) is executable.

---

## Tranche E12 — CoinLine Host integration

**Goals**

- Connect the emulated terminal's host bridge to a real running CoinLine Host instance.
- Exercise table download, table upload, and DLOG submission flows.

**Files created**

- `coinline-emu/fixtures/scenarios/table-download.json`.
- `coinline-emu/fixtures/scenarios/modem-connect.json`.

**Tests created**

- `coinline-emu/tests/integration/test_table_download_e2e.cpp`.
- `coinline-emu/tests/integration/test_dlog_submit_e2e.cpp`.

**Commands to run**

```bash
# In one shell — start the host application listening on the configured port.
# In another — run integration tests:
ctest --test-dir build --label-regex "host_integration"
```

**Acceptance criteria**

- M11 (host call attempted) and M12 (table/config storage accessed) reached deterministically.
- Tables persisted across reset.

**Blockers**

- CoinLine Host instance availability.

**Exit criteria**

- Round-trip table flow green in CI nightly job.

---

## Tranche E13 — Scenarios, artwork, CI hardening

This umbrella tranche is split across three prompts:

### E13a — Scenario runner

- Implement scenario verbs from [`scenario-runner.md`](scenario-runner.md).
- Author the scenario suite under `fixtures/scenarios/`.
- Wire evidence bundle export into every scenario run.

### E13b — Artwork and UI

- Polish `artwork/millennium.lay` with all clickable regions per [`artwork-and-layout.md`](artwork-and-layout.md).
- Author screenshot evidence captures.

### E13c — CI release hardening

- Full CI matrix per [`ci-and-release.md`](ci-and-release.md): `lint`, `unit-linux`, `unit-windows`, `integration-linux`; nightlies `nightly-acceptance`, `nightly-host-integration`, `regression-replay`.
- Regression tests: save-state round-trip + golden replay.
- Release workflow: binaries, tools, docs tarball, SHA-256 manifest, canonical evidence bundle from real firmware runs.

**Acceptance criteria**

**Exit criteria**

- Project meets the acceptance gate definition in [`acceptance-test-plan.md`](acceptance-test-plan.md).

---

## Audio tranches A0–A7 (MAME audio / supervision devices)

Parallel track to E0–E13; depends on E2 I/O map and E9 alerter stubs. Roadmap: [`audio-implementation-roadmap.md`](audio-implementation-roadmap.md).

| Audio tranche | Theme |
| ------------- | ----- |
| A0 | Map validation |
| A1 | Voiceware device |
| A2 | Routing and mute |
| A3 | Alerter output |
| A4 | Disconnect supervision |
| A5 | Call-state integration |
| A6 | Scenarios and evidence |
| A7 | CI regression |

### Tranche A0 — Map validation

**Goals:** Validate JSON maps vs `io-port-map`; emit gap report; placeholder fixtures present.

**Non-goals:** Device implementation.

**Files created:** `tests/fixtures/test_audio_maps_consistency.py` (or equivalent), optional `tools/validate-audio-maps.py`.

**Files modified:** `docs/compatibility-validation-items.md`.

**Tests:** Map consistency pytest.

**Commands:** `python -m pytest tests/fixtures/test_audio_maps_consistency.py`

**Acceptance criteria:** CI fails on orphaned port references.

**Boot milestone impact:** None.

**Exit criteria:** Gap report + ledger rows for every OPEN item.

### Tranche A1 — Voiceware device

**Goals:** `millennium_voiceware_device`; ports `0x40`/`0x42`/`0x61`; traces (`voice_reset_edge`, `voice_segment_*`, `voice_fault`).

**Non-goals:** PCM playback authenticity.

**Files created:** `millennium_voiceware.cpp/.h`, `millennium_audio_trace.cpp/.h`.

**Files modified:** `millennium_io.*`, `millennium.cpp/.h`, optional `millennium_debug.*`.

**Tests:** `tests/devices/test_voiceware_fixture_replay.cpp`; Class C requires **`coinline-mame.exe`**.

**Commands:** `tools/windows/build-mame-coinline.ps1`; `tools/windows/run-coinline-emulator.ps1 -FirmwareBinary ../firmware/flash.bin`; `tools/windows/test-coinline-emulator.ps1 -FirmwareBinary ../firmware/flash.bin`.

**Acceptance criteria:** Voiceware trace events from real firmware when ports exercised (Class C); Class A passes without firmware.

**Boot milestone impact:** **M6A**, **M11B**.

**Exit criteria:** Spec [`specs/voiceware-mame-device-implementation.spec.md`](../specs/voiceware-mame-device-implementation.spec.md) checklist satisfied.

### Tranche A2 — Routing and mute

**Goals:** `millennium_audio_route_device` + **`millennium_telephony_device`** (decode `audio-routing-state-map` bytes on modem/host path); hook/modem/voice coordination.

**Non-goals:** Inventing Z180 ports for telephony commands without `io-port-map.json` promotion.

**Files created:** `millennium_audio_route.cpp/.h`, `millennium_telephony.cpp/.h`.

**Files modified:** `millennium_io.*`, `millennium_keypad.*`, `millennium_modem.*`, `millennium_hostbridge.*`, `millennium_voiceware.*`, `millennium.cpp/.h`.

**Tests:** Route fixture replay (Class A) + **`coinline-mame.exe`** integration (Class C) per [`test-plans/audio-routing-mame-implementation-tests.md`](../test-plans/audio-routing-mame-implementation-tests.md).

**Commands:** `tools/windows/build-mame-coinline.ps1`; `tools/windows/run-coinline-emulator.ps1 -FirmwareBinary ../firmware/flash.bin`.

**Acceptance criteria:** Idle snapshot matches `fixtures/audio/audio-route-idle.json`; `telephony_command_decode` appears in traces when firmware sends routing bytes.

**Boot milestone impact:** **M6B**, **M10A**, **M11A**, **M11C**.

### Tranche A3 — Alerter output

**Goals:** Extend `millennium_audio_device`; cadence + trace; optional speaker.

**Files modified:** `millennium_audio.cpp/.h`, `millennium_io.*`.

**Tests:** `fixtures/audio/alerter-*.json` timing.

**Boot milestone impact:** **M6C**.

### Tranche A4 — Disconnect supervision

**Goals:** `millennium_supervision_device`; typed disconnect events from **`processor_to_host_codes`** via **`millennium_telephony_device`**.

**Files created:** `millennium_supervision.cpp/.h`.

**Files modified:** `millennium_telephony.*`, `millennium_modem.*`, `millennium_io.*`, `millennium_audio_trace.*`, driver glue.

**Tests:** Three disconnect fixtures (Class A) + **`coinline-mame.exe`** Class C.

**Commands:** `tools/windows/build-mame-coinline.ps1`; `tools/windows/run-coinline-emulator.ps1 -FirmwareBinary ../firmware/flash.bin`.

**Boot milestone impact:** **M8A**, **M11D**.

### Tranche A5 — Call-state integration

**Goals:** Wire all audio devices + modem + hook; scenarios.

**Files modified:** `millennium.cpp/.h`, `millennium_io.*`, `millennium_modem.*`, `millennium_keypad.*`, `millennium_voiceware.*`, `millennium_audio_route.*`, `millennium_telephony.*`, `millennium_supervision.*`, `millennium_audio.*`.

**Commands:** `tools/windows/build-mame-coinline.ps1`; scenario runner or manual emulator per [`RUNNING.md`](../RUNNING.md).

**Acceptance criteria:** Cross-trace proof matrix in [`specs/audio-call-state-integration.spec.md`](../specs/audio-call-state-integration.spec.md) satisfied with **Class C** harness only.

**Tests:** [`test-plans/audio-call-state-integration-tests.md`](../test-plans/audio-call-state-integration-tests.md).

### Tranche A6 — Evidence bundles

**Goals:** Export audio JSONL artifacts; validator updates.

**Files modified:** `tools/evidence-bundle-export/*`, `specs/evidence-bundle.spec.md`.

### Tranche A7 — CI regression

**Goals:** Firmware-run audio tests; milestone validator extensions; skip-if-no-binary policy.

**Files modified:** CI workflows, `tools/windows/test-coinline-emulator.ps1`, `validate-boot-milestones.ps1`.

**Acceptance criteria:** No fake pass when firmware missing.

---

## Cross-tranche conventions

- Every tranche must update [`compatibility-validation-items.md`](compatibility-validation-items.md) with the items it closes.
- Every tranche must produce or update at least one scenario in `fixtures/scenarios/`.
- Every tranche must add evidence-bundle assertions to the test plan it advances.
- No tranche may introduce a `coinline/` ↔ `coinline-emu/` source dependency. Use the host bridge transport.

