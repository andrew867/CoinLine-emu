# Voiceware/Scheduler Loop Analysis

Last updated: 2026-05-05

- Runs analyzed: `20260505T071654-boot-critical`, `20260505T072506-boot-critical`, `20260505T073045-boot-critical`.
- Top loop PCs are stable across all three runs: `0x00D2`, `0x00CF`, `0x00E5`, `0x0038`, with repeated `0x00AD -> 0x00CF` call pattern.
- `ei-di-events.jsonl` shows heavy `DI` (`0xF3`) and `RET` (`0xC9`) fetches in `0x00CF..0x00E5`; no sampled `EI` (`0xFB`).
- Voice command loop is periodic and stable: `port 0x0061`, `data 0xB3`, interval ~`363327` cycles, count `3044` per 180 s run.
- `voice-loop-signature.json` shows clicking maps to repeated command lifecycle, not a single long playback.
- `scheduler-loop-signature.json` shows repeated `0x00CF..0x00E5` context band and `0x0038` hits; stack spans `0x7FD0..0xCFC8` (`wide_drift`, not monotonic leak).

Classification:
- Primary loop class: `scheduler_interrupt_timer`
- Secondary coupling: `voiceware_busy_ready` / repeated voice prompt retry

Conclusion:
- Current behavior is not VFD-path bound yet; system remains in interrupt/context-save band before firmware-driven `0x0060` write (M6 gate still upstream).
