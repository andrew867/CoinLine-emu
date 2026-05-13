# CoinLine emulator — implementation status (measured)

Host: Windows, repo root **`coinline-emu`**. MSYS2 **`C:\msys64`**, **`MSYSTEM=MINGW64`** for MAME.

## Resolved inputs

| Item | Value |
|------|--------|
| Firmware binary | `../firmware/flash.bin` |
| Firmware SHA-256 | `b09f9c64817f52522cdb4a01f43cdfe5422eb65cd087defeec2906e597d60e34` |
| Firmware workspace (optional collateral, not redistributed) | `../firmware` |
| `local-inputs.json` | `build/local-inputs.json` |

## MAME pin

| Item | Value |
|------|--------|
| Root | `third-party/mame` |
| Commit | `e776c98438a465d3486c367cbad3777a6eb7902e` (mame0287 family) |
| Remote | `https://github.com/mamedev/mame.git` |

See `build/mame-version.json`.

## Build

| Artifact | Path |
|----------|------|
| MINGW64 env check | `build/logs/mingw64-env-check.log` (inside MSYS2: **`/mingw64/bin/cmake`**) |
| Bootstrap log | `build/logs/bootstrap-msys2-mame.log` |
| MSYS2 summary | `build/msys2-environment.json` |
| MAME build log | `build/logs/mame-build.log` |
| Build result | `build/build-result.json` |
| Linked EXE (copy) | `build/bin/coinline-mame.exe` |
| Upstream EXE | `third-party/mame/mamecoinline.exe` |
| MINGW64 build entry | `tools/mingw64/build-coinline-mame.sh` (overlay + `make` + copy + `build-result.json`) |

Build uses **`make SHELL=/usr/bin/bash REGENIE=1 SUBTARGET=coinline NOWERROR=1`** — **`NOWERROR=1`** avoids GCC 16 treating SFINAE-related warnings as fatal under `-Werror`. Subtarget includes Voiceware link set via **`SOUNDS["SPEAKER"]`** and **`SOUNDS["UPD7759"]`** in **`mame-overlays/scripts/target/mame/coinline.lua`**. MSYS2 **`bash`** sets **`OS=Windows_NT`** before **`make`** so MAME’s makefile picks the Windows branch (see **`tools/windows/_build_mame_inner.sh`**).

## Tranche Audio A1 (voiceware `device_t`)

| Item | Status |
|------|--------|
| **`millennium_voiceware_device`** | **Implemented** — ports **0x40** (reset via HW control), **0x42** (bank), **0x61** (phrase) per `fixtures/board/voiceware-command-map.json`. |
| **`millennium_audio_route_device`** | **Stub** subdevice (`audroute`) for `notify_voice_active`. |
| **Audio JSONL** | **`millennium_audio_trace.*`** — `schema_version` **`coinline.audio_trace/v1`**; events **`voice_reset_edge`**, **`voice_segment_start`**, **`voice_segment_complete`**, **`voice_fault`**. |
| **Env paths** | **`COINLINE_AUDIO_TRACE`**, **`COINLINE_VOICEWARE_TRACE`** — set by `tools/windows/run-coinline-emulator.ps1` to the run directory. |
| **Evidence bundle** | **`expect_audio_traces`** on `millennium_evidence_bundle_params` writes `audio/*` stubs per `docs/audio-evidence-bundle-plan.md`. |

## Tranche Audio A2 (routing, telephony decode, mute)

| Item | Status |
|------|--------|
| **`millennium_audio_route_device`** | **Implemented** — composite state per `audio-routing-state-map.json` (`call_state`, TX/RX mute, sidetone), host/modem/voice hooks, traces **`route_change`**, **`mute_change`**, **`route_conflict_resolved`**. |
| **`millennium_telephony_device`** | **Implemented** — **`host_to_processor_byte`**, map decode, traces **`telephony_host_to_processor_byte`**, **`telephony_command_decode`**, modem DCD via `notify_modem_dcd` → audio route. |
| **Shared logic** | **`millennium_audio_route_apply.*`** (no `emu.h`) — command table + `apply_effect`, used by MAME device and **Class A** unit tests. |
| **Integrations** | **Panel** `KEYMATRIX` / softkeys → TP CSI/O (`tp_process_front_panel_events`); **`notify_hook_off`** tracks debounced telephony hook (`m_tel_hook_onhook_stable`, not PIO reads). Audio routing still follows CP→TP telephony commands + voiceware/modem hooks; hook bit informs composite **`hook_off`**. Optional **`overlay_traits.data_jack_manual_keypad_active`** drives `data_jack_model` on dial/repeat opcodes. **hostbridge** `deliver_host_to_processor_byte` → telephony; **screen_update** DCD edge → `notify_modem_dcd`. |
| **Trace format** | Extended **`millennium_audio_trace.*`** for `device: telephony` / `audio_route` per `specs/audio-trace-format.spec.md`. |
| **Tests** | `test_audio_route_fixture`, `test_telephony_decode` (Class A); `test_audio_route_firmware` (Class C, **skip 77** until harness). |
| **Power-on** | `device_reset` / `reset_to_idle_fixture` matches **`fixtures/audio/audio-route-idle.json`**. |

## Tranche Audio A3 (alerter trace cadence, `millennium_audio_device`)

| Item | Status |
|------|--------|
| **Cadence tables** | **`millennium_audio_model`** — `service_beep`, `error_beep`, `user_prompt_tone` edges (ms) aligned with **`fixtures/audio/alerter-*.json`**. |
| **Trace events** | **`alerter_ready`** on `device_reset`; **`alerter_gpio_write`** for ports **0x58–0x5B**; **`alerter_tone_start` / `alerter_tone_end`** from scheduler cadence (works with **`-sound none`**). |
| **Env** | **`COINLINE_ALERTER_TRACE`** (dedicated `alerter-trace.jsonl`) plus **`COINLINE_AUDIO_TRACE`** superset — set in **`tools/windows/run-coinline-emulator.ps1`**. |
| **Tests** | **`test_alerter_cadence_fixture`**, **`test_alerter_audio_trace`** (Class A); **`test_alerter_firmware`** (Class C, **skip 77** until harness). |
| **Evidence bundle** | Stub **`audio/alerter-trace.jsonl`** is a valid **`alerter_ready`** line (not empty **`{}`**) when **`expect_audio_traces`** is set. |

## Tranche Audio A4 (disconnect supervision, `millennium_supervision_device`)

| Item | Status |
|------|--------|
| **Core FSM** | **`millennium_supervision_fsm.*`** (host-agnostic) — fuses **processor→host** status bytes (**0x60–0x8E** per **`disconnect-supervision-map.json`**) with **modem carrier** edges. |
| **MAME device** | **`millennium_supervision_device`** — traces **`supervision_status_code`**, **`disconnect_event`**, **`supervision_timeout`** via **`millennium_audio_trace.*`**. |
| **Telephony** | **`processor_to_host_byte`** + **`telephony_processor_to_host_byte`** JSONL; fan-in to supervision. **`notify_modem_dcd`** now includes **CPU cycle/PC** and drives **`on_modem_carrier`**. |
| **Host bridge** | **`deliver_processor_to_host_byte`** — transport injects bytes at the bridge only (no direct `disconnect_event` injection). |
| **Env** | **`COINLINE_SUPERVISION_TRACE`** → **`supervision-trace.jsonl`** (also in **`COINLINE_AUDIO_TRACE`**) — **`run-coinline-emulator.ps1`**. |
| **Tests** | **`test_supervision_fixture`** (Class A vs **`fixtures/audio/disconnect-*.json`** `replay_steps` / `expected_trace`); **`test_supervision_firmware`** (Class C, **skip 77** until harness). |
| **Evidence bundle** | Valid stub line in **`audio/supervision-trace.jsonl`** when **`expect_audio_traces`** is set. |

## Tranche Audio A5 (call-state integration)

| Item | Status |
|------|--------|
| **Hardware truth** | **`snapshot_integration_call_state`** derived only from **`composite_state`** (devices + telephony map effects + keypad hook + modem carrier + voiceware voice bit). No scenario-owned **`call_state`** variable. |
| **Traces** | **`route_change`** / **`mute_change`** optionally include **`call_state_if_known`** (`prompt_playback`, `connected_call`, `disconnect_detected`, …). |
| **Evidence bundle** | **`manifest.json`** supports **`mame_executable`** and **`audio`** block (`expect_audio_traces`, **`audio_milestones_claimed`**, **`trace_sha256`**) per **`docs/audio-evidence-bundle-plan.md`**; **`audio/audio-state-final.json`** stub includes **`call_state_if_known`**. Wire payload parses **`mame_executable`** / audio fields (`tools/evidence-bundle-export`). |
| **Scenarios** | **`fixtures/scenarios/audio-boot-init.json`**, **`voice-prompt-playback.json`** (new), **`disconnect-supervision.json`**. |
| **Tests** | **`test_audio_call_state_fixture`** (Class A); **`test_audio_call_state_firmware`** (Class C, **skip 77** until **`coinline-mame.exe`** harness asserts traces per **`docs/audio-ci-plan.md`**). |
| **Firmware proof** | Requires **`tools/windows/test-coinline-emulator.ps1`** / **`run-coinline-emulator.ps1`** with **`flash.bin`** — not claimed by CMake-only runs. |

## Overlay / driver

- Overlay: `tools/windows/overlay-coinline-driver.ps1` → manifest `build/overlay-manifest.json`.
- Machine short name **`cl_millennium`** (≤16 chars for MAME `SYST` macro). Run script and `coinline.lst` match this name.
- CPU: **`Z80180`** macro → `z80180_device` (see `docs/status/mame-z180-support.md`).
- Subdevices: I/O port finders use **`owner`** as search base for `KEYMATRIX` / `SECMASK`; audio device tag **`mill_audio`** (reserved-name clash avoided for `"audio"`).

## Screenshot capture run

Automated capture script: `tools/windows/run-screenshot-capture.ps1`.

### Latest measured run (`post-mmu-boot-gate`)

| Item | Value |
|------|--------|
| Run folder | `build/runs/20260504T135115-post-mmu-boot-gate/` |
| Duration | **45 s** |
| **mach_pio 0xC0** | **`mach_pio_port_h`** trace tag; read model uses **STATUS_PORT_3** cash-bit semantics (`millennium_mach_pio`) |
| **Supplemental traces** | `interrupt-trace.jsonl`, `timer-trace.jsonl`, `asci-trace.jsonl`, `reset-trace.jsonl` when env flags set |
| M6 / **0x60** | **Still absent** — milestone **M5** |

### Reference (`mmu-memory-fix`)

| Item | Value |
|------|--------|
| Run folder | `build/runs/20260504T133903-mmu-memory-fix/` |
| Duration | **45 s** (quick validation; use **180 s** for parity with harness defaults) |
| **Physical memory** | **768 KiB low overlay** — stores persist (`memory-trace.jsonl` non-empty; stack at logical **0x7FFE** → phys **0x54FFE**) |
| **MMU trace** | `mmu-translation-trace.jsonl` — logical vs physical PC/SP vs **CBR/BBR/CBAR** |
| **Stack trace** | `stack-trace.jsonl` — writes near **SP_phys** |
| M6 / port **0x60** | **Still absent** (highest milestone **M5**) |

### Reference run (`z180-visual-fix`)

| Item | Value |
|------|--------|
| Run timestamp (UTC folder) | `20260504T132707-z180-visual-fix` |
| Run folder | `build/runs/20260504T132707-z180-visual-fix/` |
| Firmware SHA-256 | `b09f9c64817f52522cdb4a01f43cdfe5422eb65cd087defeec2906e597d60e34` |
| MAME commit | `e776c98438a465d3486c367cbad3777a6eb7902e` |
| **I/O trace** | **`io-trace.jsonl`** populated; internal MMU lines use decoded tags (**`z180_mmu_bbr`**, etc.) |
| **MMU readback** | Port **0x0039** reads **`0x00`/`0x4D`** per firmware state — **not** erroneous **`0xFF`** in trace |
| **cpu / z180 / memory traces** | **`cpu-trace.jsonl`**, **`z180-register-trace.jsonl`**, **`memory-trace.jsonl`** emitted |
| GUI screenshots | **`boot-normal-start.png`** and follow-ups — **`screenshot_evidence_label`:** **`real_gui_screenshot`** |
| M10 / M6 | **not reached** / **not fired** — highest milestone **M5**; **no `vfd_data`** lines |
| INSTALL | **install_blocked** — harness unchanged |
| Advertising scroll | **ad_scroll_not_observed** — no `0x0060` VFD writes in trace |
| Evidence index | `evidence-summary.json`, `screenshot-capture-report.md` |

Scenario fixture added: `fixtures/scenarios/install-and-ad-scroll.json` (copied per run as `scenario-install.json`).

## Live run (proof of execution)

Example successful run directory:

`build/runs/20260503T221924/`

| Artifact | Path |
|----------|------|
| Run summary | `run-summary.json` |
| Boot trace | `boot-trace.jsonl` |
| I/O trace | `io-trace.jsonl` / `io-trace.log` |
| Unknown ports | `unknown-port.jsonl` / `unknown-ports.json` |
| Milestones JSON | `boot-milestones.json` |

Example command (from `run-summary.json`):

`coinline-mame.exe cl_millennium -rompath ../firmware -video none -sound none -nothrottle -seconds_to_run 120`

### Milestones from real execution

From **`boot-trace.jsonl`** in that run: **M0–M5** observed (firmware hash, reset opcode, startup PC, RAM/reg snapshot, keypad device activity). **M6–M10** not observed within **120 s**.

Full sequence validation (`validate-boot-milestones.ps1` without relaxing rules) **fails** until M6–M10 appear — see **`boot-blocker.md`** in the same run folder.

## Telephony processor (PCD3349A / 8048 backend)

Summary of behavioral ROM location, hook transition + steady-state sequencing toward the CP, and the **internal timer start** requirement for deferred hook bytes: [**TP-PCD3349A-BEHAVIORAL-ROM-STATUS.md**](TP-PCD3349A-BEHAVIORAL-ROM-STATUS.md).

## Honest test labeling

- **CMake / JSON / scenario-only tests** under `tests/` that never spawn `coinline-mame.exe` are **not** “MAME integration” tests.
- **PowerShell integration scripts** under `tests/integration/*.Tests.ps1` check artifacts **after** a real run (they require `boot-trace.jsonl` and firmware path); they do not replace MAME execution.

## Upstream carry patch

- **`third-party/mame/src/emu/addrmap.h`**: `address_map_entry::rw` write side uses `*make_pointer<V>(obj)` so `.rw(state, …)` delegates compile.

## Next three tasks

1. Use **`build/runs/20260503T221924/boot-blocker.md`** and **`unknown-ports.json`** to extend I/O (one device at a time), rebuild, rerun.
2. Increase **`-RunSeconds`** or fix blocking ports until **M6–M10** lines appear in **`boot-trace.jsonl`**, then rerun **`validate-boot-milestones.ps1`**.
3. Optional: fix **`capture-mame-screenshot.ps1`** for this host’s System.Drawing API or use MAME’s snapshot path for PNG evidence.
