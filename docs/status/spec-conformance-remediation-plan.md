# CoinLine Emulator Spec Conformance Remediation Plan

## Objective

Close all identified conformance gaps between the emulator implementation and the internal terminal spec set, remove alternate/incorrect behavior paths, and produce schema-valid evidence for each normative vector.

## Remediation Matrix

### P0 (Critical path)

- **Spec area:** `terminal_17`, `terminal_19` telephony runtime failure
  - **Gap:** `0x38` path clears health too early and bypasses required escalation/reset sequence.
  - **Code touchpoints:** `src/mame/coinline/millennium_state.cpp`
  - **Fix:** Implement strict consecutive-miss FSM (miss threshold -> alarm set -> reset signal -> clear on valid recovery response only).
  - **Validation:** Add tests for 1/2/3+ miss progression, alarm latch/clear, and reset emission.

- **Spec area:** `terminal_25` evidence schema
  - **Gap:** Required `user-io-*` artifact names/fields are not emitted.
  - **Code touchpoints:** `src/mame/coinline/millennium_state.cpp`, `src/mame/coinline/millennium_evidence_bundle.cpp` and related emitters
  - **Fix:** Emit `user-io-trace.jsonl`, `user-io-summary.json`, conditional `user-io-failures.md`; include required fields (`vector_results`, `pass_fail_summary`, counters).
  - **Validation:** JSON schema tests, golden artifact tests, CI check for required files.

- **Spec area:** `terminal_25` harness input schema
  - **Gap:** No strict parser/validator for required harness fields.
  - **Code touchpoints:** Harness input loading/ingestion paths
  - **Fix:** Validate `run_id`, `profile_traits.quick_access_key_count`, `profile_traits.has_11_line_softkeys`, `enabled_vectors`, plus constraints.
  - **Validation:** Unit tests for valid and invalid payloads with deterministic errors.

- **Spec area:** `terminal_21_user_io_profile_matrix.yaml` + `terminal_25`
  - **Gap:** Board fixtures do not match normative quick-access key counts.
  - **Code touchpoints:** `fixtures/board/board-profile-11line-vfd.json`, `fixtures/board/board-profile-2line-vfd.json`, `src/mame/coinline/millennium_board_profile.cpp`
  - **Fix:** Align counts to normative profiles (10 / 5), enforce constraints in parser.
  - **Validation:** Fixture conformance test for deterministic profile resolution.

### P1 (High conformance gaps)

- **Spec area:** `terminal_00` telephony init boot code selection
  - **Gap:** Boot code path effectively hardcoded to one branch.
  - **Code touchpoints:** `src/mame/coinline/millennium_state.cpp`
  - **Fix:** Implement conditional `0x70` vs `0x72` selection per boot-state semantics.
  - **Validation:** Boot vectors covering both branches.

- **Spec area:** `terminal_01` modem NCC runtime/session behavior
  - **Gap:** Codec exists, runtime FSM/retry/timeout behavior missing.
  - **Code touchpoints:** `src/mame/coinline/millennium_modem_ncc.cpp` (+ runtime model)
  - **Fix:** Add session/call progress FSM on top of framing codec.
  - **Validation:** Nominal + retry/timeout/recovery scenario tests.

- **Spec area:** `terminal_02` coin validator protocol
  - **Gap:** Pulse abstraction does not implement command/response protocol.
  - **Code touchpoints:** `src/mame/coinline/millennium_coin_model.cpp`, `src/mame/coinline/millennium_coin.cpp`
  - **Fix:** Implement UART command parser/serializer and required status/error semantics.
  - **Validation:** Vector tests for command catalog and status responses.

- **Spec area:** `terminal_03` magstripe reader
  - **Gap:** Simplified stream model lacks full track-2 decode/error taxonomy.
  - **Code touchpoints:** `src/mame/coinline/millennium_card_model.cpp`
  - **Fix:** Add track-2 parity/LRC/sentinel decode path and error classes.
  - **Validation:** Fixture tests for nominal and error variants.

- **Spec area:** `terminal_04` smart card
  - **Gap:** ATR playback only; negotiation/fallback not implemented.
  - **Code touchpoints:** `src/mame/coinline/millennium_smartcard_model.cpp`
  - **Fix:** Implement ATR parse, protocol negotiation, fallback, and gating semantics.
  - **Validation:** Card/profile matrix tests by protocol path.

### P2 (Major platform fidelity)

- **Spec area:** `terminal_16` RTOS startup scheduler
  - **Gap:** Required startup task model/rendezvous not implemented as specified.
  - **Code touchpoints:** `src/mame/coinline/millennium_state.cpp` (+ dedicated scheduler model)
  - **Fix:** Implement explicit startup task-list materialization and rendezvous semantics.
  - **Validation:** Deterministic startup-order and milestone trace assertions.

- **Spec area:** `terminal_15` Z180 boot/MMU
  - **Gap:** Simplified boot model misses full branch fidelity.
  - **Code touchpoints:** `src/mame/coinline/millennium_z180_boot_init_model.cpp`
  - **Fix:** Add complete MMU mapping/init sequence for banked and non-banked paths.
  - **Validation:** Boot branch vectors with per-step markers.

- **Spec area:** `terminal_14_voice_synthesis`
  - **Gap:** Device-level behavior exists; task signaling/state contract incomplete.
  - **Code touchpoints:** `src/mame/coinline/millennium_voiceware.cpp`, `src/mame/coinline/millennium_state.cpp`
  - **Fix:** Add explicit voice synthesis FSM and timeout/completion alarm contract.
  - **Validation:** Completion/timeout/late-completion tests.

- **Spec area:** `terminal_09` data jack
  - **Gap:** Reduced state model vs required state/signal matrix.
  - **Code touchpoints:** `src/mame/coinline/millennium_terminal_peripherals_model.cpp`
  - **Fix:** Expand to full declared state machine and transitions.
  - **Validation:** Transition-table scenario tests.

- **Spec area:** `terminal_23` overlay arbitration
  - **Gap:** Policy gating exists; full ordered consume/decline chain incomplete.
  - **Code touchpoints:** `src/mame/coinline/millennium_state.cpp`
  - **Fix:** Implement explicit ordered overlay arbitration dispatcher.
  - **Validation:** Single-winner and fallback-order tests.

### P3 (Completeness and hardening)

- **Spec area:** `terminal_05` disconnect supervision
  - **Gap:** Timing window fidelity reduced.
  - **Code touchpoints:** `src/mame/coinline/millennium_supervision_fsm.cpp`
  - **Fix:** Add explicit cutoff/wink/defer timing windows.
  - **Validation:** Clock-controlled timing vector tests.

- **Spec area:** `terminal_07` 11-line VFD specifics
  - **Gap:** Some 11-line-specific semantics are not explicit.
  - **Code touchpoints:** `src/mame/coinline/millennium_vfd_model.cpp`
  - **Fix:** Add explicit page/softkey/active-line/blink model elements.
  - **Validation:** Golden-frame fixture tests.

- **Spec area:** `terminal_08` peripheral health
  - **Gap:** Recovery/reset/debounce semantics partially implemented.
  - **Code touchpoints:** `src/mame/coinline/millennium_terminal_peripherals_model.cpp`
  - **Fix:** Implement missing recovery/latch/debounce clear rules.
  - **Validation:** Fault injection and recovery vectors.

- **Spec area:** `terminal_10`, `terminal_11`, `terminal_12`, `terminal_13`
  - **Gap:** Partial conformance in each module.
  - **Code touchpoints:** `src/mame/coinline/millennium_terminal_peripherals_model.cpp`
  - **Fix:** Fill missing special paths and record payload semantics.
  - **Validation:** Expand `tests/devices/test_terminal_peripherals_vectors.cpp` with spec-named vectors.

- **Spec area:** `terminal_20`, `terminal_22`, `terminal_24` evidence closure
  - **Gap:** Behavior partly present; vector-grade proof incomplete.
  - **Code touchpoints:** `src/mame/coinline/millennium_state.cpp`, harness/evidence emitters
  - **Fix:** Emit per-vector outcomes with timing/expected-byte assertions under Terminal 25 schema.
  - **Validation:** Conformance suite producing schema-valid vector results.

## Phased Execution Plan

1. **Phase 1 - Governance and evidence baseline**
   - Implement Terminal 25 harness input validation and evidence schema outputs.
   - Align profile fixtures and enforce profile constraints.

2. **Phase 2 - Highest-risk behavior correction**
   - Fix Terminal 17/19 runtime watchdog/reset flow.
   - Fix Terminal 00 boot-code branch selection.

3. **Phase 3 - Core protocol parity**
   - Implement/upgrade Terminals 01, 02, 03, 04 protocol/runtime behavior.

4. **Phase 4 - Platform/runtime fidelity**
   - Implement/upgrade Terminals 15, 16, 14, 09, 23.

5. **Phase 5 - Complete remaining vectors and harden evidence**
   - Close Terminals 05, 07, 08, 10, 11, 12, 13, 20, 22, 24.
   - Ensure all vectors produce Terminal 25-compliant outputs.

## Exit Criteria

- Every normative vector has deterministic automated test coverage.
- CI passes with no failing normative vectors.
- Terminal 25 artifacts and JSON fields are present and schema-valid for each conformance run.
- No alternate code path bypasses required escalation, reset, or arbitration contracts.
- Profile resolution is deterministic and spec-conformant for all supported board profiles.

## Suggested Tracking Fields (for execution updates)

- Owner
- Status (`todo`, `in_progress`, `blocked`, `done`)
- Target PR
- Test IDs / vector IDs covered
- Evidence artifact links (run folder relative path)
- Open risks / assumptions
