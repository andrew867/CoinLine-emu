# M7C OOS Message Selector Analysis

## Scope
- Focus area: `oos_message_selector` path selecting `telephony_not_responding`.
- Runs analyzed: `build/runs/20260506T100452-boot-critical`, `build/runs/20260506T100744-boot-critical`, `build/runs/20260506T100936-boot-critical`.
- Capture duration per run: 45 seconds.

## Selector condition currently observed
- The selector continues to choose `telephony_not_responding` whenever current VFD rows still contain `Telephony board is` and `not responding`.
- This remains true even when:
  - `telephony_ready_state` is true (`ready_candidate=true`)
  - alarm clear is observed (`alarm_state=cleared`)
  - retry/timeout are no longer active (`retry_active=false`, `timeout_pending=false`)
  - `service_mode_state` / craft / door-lock-vault fields are all zero in traces.

## Inputs traced and findings
- `alarm_condition_update`: clear observed after latch (`C4` clear path present).
- `telephony_ready_state`: true repeatedly after clear.
- `telephony_retry_state`: retry and timeout not forcing after clear.
- `service_mode_state`: service/craft/door-lock-vault not forcing.
- `display_message_cache`: repeated `display_cache_hit` + `display_message_unchanged`.
- `service_display_refresh`: refresh signals continue to fire but selector result does not change.

## Implemented fixes in this loop
- Fix A (timing/order propagation): publish status (`C0`) on active post-clear poll path with refresh signal so clear+ready are presented together to service/OOS evaluation.
- Fix B (line defaults): boot default for simulated line-answer normalized to unasserted state.

## Effect of fixes
- Fix A changed behavior evidence:
  - `reason` changed from `uart_alias_poll_0x01_error_report_only` to `uart_alias_poll_0x01_publish_status_after_clear`.
  - `C0_sent=true` observed alongside `C4_sent=true`.
  - service refresh now logs `error_report_poll_status_publish`.
- Fix B changed line mode evidence:
  - `line_mode_bits` moved from `0x00000004` to `0x00000000`.
- Despite both changes, final selector output remains `telephony_not_responding`.

## Answers to required questions
- Exact condition selecting telephony-not-responding:
  - Selector still resolves to `telephony_not_responding` because current VFD text remains the same on each selector pass.
- Is selector reading alarm/ready/retry/timeout/service/craft/lock-cache:
  - Alarm and ready are observed but not sufficient to change result.
  - Retry/timeout are not active after clear.
  - Service/craft/door-lock-vault traces do not indicate forcing bits.
  - Display cache shows persistent hit/unchanged state.
- Which condition remains true after clear + status:
  - `display_message_cache` + unchanged `current_vfd_text` remains true.
- Is telephony-not-responding alarm cleared from selector perspective:
  - Yes (`alarm_state=cleared` observed while message still selected).
- Separate OOS reason bit still set:
  - Source-correlated trace indicates yes at selector outcome level (message remains selected).
- Retry/timeout forcing:
  - No, not after clear.
- Service/craft forcing:
  - No evidence in current traces.
- UI cached message needs invalidation:
  - Strongly indicated by repeated cache-hit/unchanged events.
- Is display queue replacing text with same message due to selector:
  - Yes, selector outcome remains `telephony_not_responding` on repeated re-evaluation.
- Exact emulated behavior that should change selector:
  - `service_display_refresh` must produce a non-not-responding replacement after clear+ready, i.e. trigger true OOS reason recompute path that emits alternate/no OOS message and invalidates cached not-responding selection.

## Current blocker
- `oos_message_check_still_selects_not_responding` with persistent display-cache hit/unchanged outcome after clear+ready refresh.
