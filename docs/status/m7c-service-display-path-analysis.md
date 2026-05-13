# M7C Service Display Path Analysis (2026-05-06)

## Scope
- Repo run baseline: `build/runs/20260506T005800-boot-critical` (120s, uart profile).
- Policy matrix runs: `20260506T010242` (`immediate`), `20260506T010324` (`latch_then_clear`), `20260506T010503` (`withhold_until_not_responding_seen`).
- Post-matrix latch/clear fix run: `build/runs/20260506T010756-boot-critical` (`latch_then_clear`).

## Observed UI State
- Final VFD remains:
  - `Telephony board is`
  - `not responding`
- `M7C` not emitted in `boot-milestones.json`.

## Source Path Mapping
- `CONTTELC.C`: `TELEPHONY_ERROR_REPORT` path calls clear on `ALM_TEL_NOT_RESPONDING`.
- `TERMSUB2.C`: alarm-clear path can set `TERMFG_TELEPHONY_UP` and request service-level update.
- `SERVTASK.C`: `SERVS_UPDATE_LEVEL` / `SERVS_CHECK_OOS_MSG` control whether not-responding message is replaced.
- `INITASK.C` + `CONTTEL2.C`: `INIS_GOT_TEL_INF` and init ordering participate in telephony-up progression.

## Trace Evidence
- `telephony-parser-trace.jsonl`: checksum-valid `C4` and `C0` accepted.
- `service-display-trace.jsonl`: `service_display_refresh_candidate` emitted from alarm-clear path.
- `termflag-trace.jsonl`: `TERMFG_TELEPHONY_UP` candidate transition (`false -> true`) inferred.
- `rtos-signal-trace.jsonl`: `INIS_GOT_TEL_INF` candidate emitted.
- `alarm-condition-trace.jsonl`: now shows `alarm_latch_candidate` followed by `alarm_clear_candidate`.
- `service-task-trace.jsonl`: no observed `SERVS_UPDATE_LEVEL` or `SERVS_CHECK_OOS_MSG` execution event.
- `telephony-ready-decision-trace.jsonl`: shows `c4_withheld_waiting_for_latch` then `c4_released_after_latch` under policy control.

## Concrete Fixes Applied This Cycle
- Added `COINLINE_TEL_RESPONSE_POLICY`:
  - `immediate`
  - `latch_then_clear`
  - `withhold_until_not_responding_seen`
  - `withhold_until_retry`
  - `withhold_until_timeout`
- Set default policy for `COINLINE_TRACE_PROFILE=uart` to `latch_then_clear`.
- Added policy-driven trace events:
  - `not_responding_display_seen`
  - `c4_withheld_waiting_for_latch`
  - `c4_released_after_latch`
  - `alarm_clear_candidate`
- Added latch inference on observed not-responding display branch to force clear-after-latch ordering.

## Current Interpretation
- Telephony parser acceptance is no longer the gate (M7B already true).
- Current gate is service-display propagation after telephony frames.
- Alarm ordering blocker is resolved (latch then clear is now observed).
- Current blocker is downstream: service/UI refresh path still does not advance after clear/signal, so VFD remains on `TELEPHONY_NOT_RESPONDING_MSG`.
