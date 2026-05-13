# Boot milestones

This document defines the boot milestone ladder M0–M12 used by every test, scenario, and evidence bundle in `coinline-emu`. Milestones are observable events; each one corresponds to a verifiable trace signal and a deterministic test.

## Milestone table

| Milestone | Evidence | Trace signal | Required emulator support | Test | Status |
| --------- | -------- | ------------ | ------------------------- | ---- | ------ |
| **M0** Firmware binary loaded | ROM region populated, SHA-256 matches `fixtures/firmware/firmware-hashes.json` | Pre-execution log entry: `firmware loaded sha256=… size=…` | Firmware loader (`specs/firmware-loader.spec.md`) | `tests/boot/test_firmware_load.cpp` | planned |
| **M1** Reset vector executed | First instruction fetched from the reset vector address | First boot-trace entry: `pc=… opcode=…` at the configured reset vector | Z180 CPU + memory map | `tests/boot/test_reset_vector.cpp` | planned |
| **M2** Startup code begins | PC enters the startup symbol or first jump from the reset vector | Boot-trace entry: `pc=startup` | Z180 CPU + memory map | `tests/boot/test_first_instructions.cpp` | planned |
| **M3** RAM initialized | RAM cleared/zeroed; stack pointer set | I/O-trace shows RAM writes spanning the RAM region; SP register set | Memory map (RAM region) | `tests/boot/test_ram_init.cpp` | planned |
| **M4** Z180 registers initialized | MMU, ASCI, PRT, INT controller, DMA configured | Trace dump of Z180 internal-register writes | Z180 internal peripherals (`docs/z180-internal-peripherals.md`) | `tests/devices/test_z180_*.cpp` | planned |
| **M5** First device driver init observed | Firmware writes to the first I/O port mapped to a device | I/O-trace entry: `pc=… port=… write=…` | I/O map + at least one device | `tests/devices/test_first_device_init.cpp` | planned |
| **M5V** First voiceware phrase command | Firmware writes **`VOICE_SYNTHESIS_CODE_ADDR` (0x0061)** — phrase/sample ID | Boot trace: `milestone=M5V`, `phrase` hex, `pc` | `voice_phrase_w` + boot trace | *(voiceware trace tests)* | in driver |
| **M6** VFD write observed | Firmware writes a printable string or known command to the VFD | VFD device captures non-empty buffer | VFD device | `tests/devices/test_vfd_command_decode.cpp` | planned |
| **M7** Keypad scan observed | Firmware reads the keypad matrix at expected cadence | Keypad device records reads | Keypad device | `tests/devices/test_keypad_scan.cpp` | planned |
| **M8** UART/modem init observed | Firmware programs ASCI registers and asserts modem signals | ASCI register writes; modem state transitions to ready | Z180 ASCI + modem device | `tests/devices/test_modem_state_machine.cpp` | planned |
| **M9** RTOS scheduler entry observed | Firmware enters its scheduler / main loop | PC enters the scheduler symbol (or the firmware's idle wait) | RTOS init source-map (``) | `tests/boot/test_rtos_entry.cpp` | planned |
| **M10** Idle loop / display reached | Firmware's idle display matches `fixtures/display/vfd-*-idle.json` | VFD buffer matches reference; PC is in idle loop | All E0–E9 devices | `tests/integration/test_boot_to_idle.cpp` | planned |
| **M11** Host call / session attempted | Firmware initiates a call via the modem; bytes appear at the host bridge | Modem state transitions; host-bridge transcript shows TX bytes | Modem + host bridge | `tests/integration/test_modem_connect.cpp` | planned |
| **M12** Table / config storage accessed | Firmware reads or writes the table storage region | NVRAM/table-storage trace shows accesses; downloaded tables persisted | NVRAM device + table storage | `tests/integration/test_table_download_e2e.cpp` | planned |

### Extended audio / supervision milestones

These **extend** M0–M12 and are asserted via [`specs/audio-trace-format.spec.md`](../specs/audio-trace-format.spec.md) and [`docs/audio-boot-integration-plan.md`](audio-boot-integration-plan.md). They require the Audio A-tranche devices per [`docs/audio-mame-device-architecture.md`](audio-mame-device-architecture.md).

| Milestone | Evidence trace | Required device implementation | Test plan | Status |
| --------- | -------------- | ------------------------------ | --------- | ------ |
| **M6A** Voiceware initialization observed | `voiceware-trace.jsonl`: first `voice_segment_*` or armed latch event after reset discipline | `millennium_voiceware_device` | [`test-plans/voiceware-mame-implementation-tests.md`](../test-plans/voiceware-mame-implementation-tests.md) | planned |
| **M6B** Audio routing default state observed | `audio-trace.jsonl`: `route_state` matches `fixtures/audio/audio-route-idle.json` after reset | `millennium_audio_route_device` | [`test-plans/audio-routing-mame-implementation-tests.md`](../test-plans/audio-routing-mame-implementation-tests.md) | planned |
| **M6C** Alerter initialization observed | `alerter-trace.jsonl`: ready/idle or first tone-arming boundary | `millennium_audio_device` (extended) | [`test-plans/alerter-audio-mame-implementation-tests.md`](../test-plans/alerter-audio-mame-implementation-tests.md) | planned |
| **M8A** Line / audio supervision initialized | `supervision-trace.jsonl` or correlated `io-trace` on supervision ports | `millennium_supervision_device` | [`test-plans/disconnect-supervision-mame-implementation-tests.md`](../test-plans/disconnect-supervision-mame-implementation-tests.md) | planned |
| **M10A** Idle audio route state confirmed | `route_state` idle while M10 VFD idle achieved | `millennium_audio_route_device` + VFD | [`test-plans/audio-routing-mame-implementation-tests.md`](../test-plans/audio-routing-mame-implementation-tests.md) | planned |
| **M11A** Off-hook route transition observed | `route_change` correlated with hookswitch off-hook in `io-trace.jsonl` | route + keypad/hook | [`test-plans/audio-routing-mame-implementation-tests.md`](../test-plans/audio-routing-mame-implementation-tests.md) | planned |
| **M11B** Voice prompt playback observed | `voice_segment_start` + `voice_segment_complete` (or fault with recovery) | voiceware | [`test-plans/voiceware-mame-implementation-tests.md`](../test-plans/voiceware-mame-implementation-tests.md) | planned |
| **M11C** Microphone mute/unmute observed | `mute_change` on mic path | route device | [`test-plans/audio-routing-mame-implementation-tests.md`](../test-plans/audio-routing-mame-implementation-tests.md) | planned |
| **M11D** Disconnect supervision event observed | `disconnect_event` in `supervision-trace.jsonl` during teardown scenario | supervision + modem | [`test-plans/disconnect-supervision-mame-implementation-tests.md`](../test-plans/disconnect-supervision-mame-implementation-tests.md) | planned |

## Why these milestones

Each milestone is the smallest observable bring-up event whose absence indicates a missing emulator capability. The ladder is the same one used by experienced bring-up engineers when bringing up a new MAME machine driver: observe each milestone, record what it took to get there, and add tests that pin it.

## Trace signals

A boot trace produced by the emulator includes one entry per milestone:

```jsonc
[
  {"milestone": "M0", "ts": "...", "sha256": "...", "size": ...},
  {"milestone": "M1", "ts": "...", "pc": "0x0000", "opcode": "..."},
  {"milestone": "M2", "ts": "...", "pc": "0x...."},
  {"milestone": "M3", "ts": "...", "ram_writes": ..., "sp": "0x...."},
  {"milestone": "M4", "ts": "...", "registers": {"mmu": "...", "asci": "...", "prt": "...", "int": "...", "dma": "..."}},
  {"milestone": "M5", "ts": "...", "device": "...", "pc": "0x...."},
  {"milestone": "M6", "ts": "...", "vfd": "..."},
  {"milestone": "M7", "ts": "...", "keypad_scan_count": ...},
  {"milestone": "M8", "ts": "...", "asci_state": "...", "dcd": false, "cts": true},
  {"milestone": "M9", "ts": "...", "scheduler_pc": "0x...."},
  {"milestone": "M10", "ts": "...", "vfd": "...", "idle": true},
  {"milestone": "M11", "ts": "...", "host_bridge_tx_bytes": ...},
  {"milestone": "M12", "ts": "...", "table_storage_writes": ...}
]
```

This shape is part of the evidence bundle (`specs/evidence-bundle.spec.md`).

## Status tracking

The `Status` column in the milestone table mirrors the row-level status in [`compatibility-validation-items.md`](compatibility-validation-items.md). Each milestone has a status of:

- `planned` — emulator support not yet implemented.
- `passes-in-ci` — milestone reproducibly observed in CI on the latest firmware binary.
- `regressed` — previously passing, now failing; triage required.
- `field-validated` — observed on physical hardware (independent of CI).

`field-validated` is **never** a prerequisite for `passes-in-ci`. Hardware certification is independent.

## Failure modes

If a milestone is not reached:

| Failure | Likely cause | Fix path |
| ------- | ------------ | -------- |
| M0 missing | Firmware path wrong or hash mismatch | Verify `../firmware/flash.bin` (or `COINLINE_FIRMWARE`) and `firmware-hashes.json`. |
| M1 missing | Reset vector address mismatch | Cross-check ``. |
| M2 missing | Reset-vector indirection (e.g. jump table) not modeled | Add MMU / banking support per `docs/z180-internal-peripherals.md`. |
| M3 missing | RAM region not writable | Check memory map; verify wait-state config. |
| M4 missing | Z180 internal-peripheral write decode wrong | Cross-check ``. |
| M5 missing | First device's I/O ports not yet mapped | Add I/O map entry for the device per `docs/io-port-map.md`. |
| M6 missing | VFD command decoding wrong | Cross-check ``. |
| M7 missing | Keypad I/O mapping wrong | Cross-check ``. |
| M8 missing | ASCI register defaults wrong | Cross-check `docs/z180-internal-peripherals.md` (ASCI section). |
| M9 missing | RTOS scheduler symbol unknown | Cross-check ``. |
| M10 missing | Idle dependency missing | Diff `compatibility-validation-items.md` for any `pending` rows. |
| M11 missing | Modem state machine or host-bridge transport bug | Replay `fixtures/modem/clean-connect.hex`; inspect host-bridge log. |
| M12 missing | Table storage region not wired or NVRAM persistence broken | Cross-check `docs/nvram-and-table-storage.md`. |

## Cross-references

- [`architecture.md`](architecture.md) — how milestones map to subsystems.
- [`evidence-bundles.md`](evidence-bundles.md) — how trace signals are captured.
- [`scenario-runner.md`](scenario-runner.md) — `wait_for_milestone` verb.
