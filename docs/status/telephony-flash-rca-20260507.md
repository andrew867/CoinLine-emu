# Telephony Flash RCA - 2026-05-07

## Executive Summary

Intermittent flashes of `Telephony board is not responding` are reproducible during acceptance runs. Because this text is firmware-driven, the first-order interpretation is a CP↔TP contract failure (or contract-observable failure), not a UI-originating defect. In the analyzed run (`20260507T101737-nis-craft-install-acceptance`), the CP-visible telephony stream shows only sparse TP response milestones (`0x72` once, `0xC4` once, `0xC0` once) and then no sustained response cadence while firmware repeatedly renders not-responding windows. This indicates an **emulated TP protocol/cadence gap visible to firmware**, with validator logic issues as a secondary amplifier.

Confidence:
- Primary root cause (CP↔TP protocol/cadence gap visible to firmware): **high**
- Secondary root cause (acceptance gate criteria mismatch): **high**
- Tertiary contributor (display/message arbitration sensitivity): **medium**

## Scope And Inputs

Primary artifacts:
- `build/runs/20260507T101737-nis-craft-install-acceptance/acceptance-summary.json`
- `build/runs/20260507T101737-nis-craft-install-acceptance/front-panel-input-source-trace.jsonl`
- `build/runs/20260507T101737-nis-craft-install-acceptance/vfd-trace.jsonl`
- `build/runs/20260507T101737-nis-craft-install-acceptance/telephony-ready-decision-trace.jsonl`
- `build/runs/20260507T101737-nis-craft-install-acceptance/telephony-runtime-conversation-trace.jsonl`
- `src/mame/coinline/millennium_state.cpp`

Acceptance summary for this run:
- `A1=false A2=false A3=true A4=false A5=false A6=false`
- `runtime_conversation_failed=true`
- `telephony_board_fault_after_oos=true`
- Final VFD text: `*  out of service  *`

## Symptom Ledger

Observed `vfd_not_responding_seen` bursts in `vfd-trace.jsonl` (firmware-rendered):
- Burst 1: cycles `75978115 -> 76151121`
- Burst 2: cycles `146855071 -> 147027696`
- Burst 3: cycles `212097565 -> 212271816`

At these windows, runtime trace fields show:
- `alarm_state=false`
- `retry_counter=0`
- `timeout_counter=0`
- `runtime_poll_count=0` (in these display updates)

Protocol-cadence evidence from `tp-csio-raw-trace.jsonl`:
- `tp_to_cp_byte 0x72`: 1 occurrence
- `tp_to_cp_byte 0xC4`: 1 occurrence
- `tp_to_cp_byte 0xC0`: 1 occurrence
- No sustained TP->CP status/error cadence beyond those milestones

## Timeline (Condensed)

1. Telephony init ACK appears (`0x72`) early in run.
2. First `not responding` display burst appears while hook is off-hook, then transitions to out-of-service text.
3. Craft keypad sequence `2727378` reaches completion in trace (`craft_code_progress.complete=true`).
4. Later short `not responding` bursts recur after out-of-service windows.
5. Validator fails A1/A2/A4/A5/A6 despite trace evidence of hook events and full craft sequence progression.

## Hypotheses And Falsification

### H1: CP firmware perceives TP non-response due emulated protocol/cadence gap
Evidence checked:
- `tp-csio-raw-trace.jsonl` TP->CP byte cadence and milestone responses
- `telephony-runtime-conversation-trace.jsonl` handshake and runtime events
- firmware-rendered VFD transitions during sparse TP response periods

Result:
- Telephony milestone responses occur, but sustained response cadence is absent in the analyzed window.
- Firmware repeatedly renders not-responding banner in those periods.
- Even without emulator-side alarm latch, firmware-visible transport behavior is sufficient for user-visible fault messaging.

Status: **supported (primary)**.

### H2: Display/message arbitration heuristics can amplify or prolong visible fault windows
Evidence checked:
- `millennium_state.cpp` VFD not-responding heuristic and gate logic
- `vfd-trace.jsonl` character-level writes during flash

Key code behavior:
- Not-responding classification uses:
  - `line0 contains telephony AND line1 contains not respo`, **or**
  - `line1 contains not respo` alone.
- This permits transient/partial display states to enter `not_responding` path without requiring alarm latch.

Result:
- Burst behavior matches char-by-char text rendering windows and repeated message transitions.
- Alarm remains clear while message appears.

Status: **supported (contributor, not primary)**.

### H3: Acceptance gate misclassifies valid progression
Evidence checked:
- `front-panel-input-source-trace.jsonl`
- Acceptance script logic in `tools/mingw64/run-nis-craft-install-acceptance.sh`

Findings:
- Hook transitions are observed by MAME (`input_hook_offhook_seen_by_mame`, `input_hook_onhook_seen_by_mame`).
- Craft digits progress to completion (`digits=2727378`, `complete=true`).
- Validator flags A2/A4/A5 false due stricter firmware-read/marker expectations and broad post-OOS not-responding rejection criteria.

Status: **supported (secondary root cause)**.

### H4: Input-sequence race creates short regressions
Evidence checked:
- Timing and ordering of scripted hook and keypad events
- Repeated hook transitions later in run

Result:
- Race potential exists (multiple later hook/keymatrix toggles), but current evidence is insufficient to make this the dominant root cause.

Status: **plausible, unproven contributor**.

## Root Cause Statement

1. **Primary:** Firmware-visible CP↔TP contract/cadence is incomplete in the analyzed run (sparse TP responses without sustained runtime progression), causing firmware to render not-responding windows.
2. **Secondary:** Acceptance gating marks several false negatives despite trace evidence of hook and craft-sequence progression.
3. **Tertiary:** Display/message arbitration heuristics can magnify transient protocol stalls into visible flashing behavior.

## Risks If Unchanged

- False incident flags during acceptance and field diagnostics.
- Operator confusion due transient critical messaging.
- Under-reporting of real root cause due validator noise.

## Residual Uncertainty

- Exact firmware-intended debounce/hysteresis behavior for transitioning from OOS to not-responding under rapid message refresh.
- Whether later hook/keymatrix transitions in long runs can independently trigger additional false windows.

## No-Code Recommendations (Pre-Implementation)

- Restore TP response cadence fidelity first (runtime CP poll handling and TP reply progression).
- Then require stronger fault evidence before classifying display as not-responding.
- Debounce/hysteresis for user-visible not-responding message transitions.
- Align acceptance gates with trace-grounded success criteria and separate diagnostics from hard fail criteria.
