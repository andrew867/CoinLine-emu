# Voiceware Interrupt Line Debug

Last updated: 2026-05-05

## Source-backed expectation

- Firmware source uses INT0 API (`enable_voice_synthesis_int`, `disable_voice_synthesis_int`) and manipulates `ITC.ITE0`.
- Completion semantics are tied to voice segment end and ISR dispatch.

## Emulator line model changes in this prompt

1. Added explicit opcode-level event evidence (`ei-di-events.jsonl`).
2. Removed deferred INT0 queue behavior; completion now emits edge pulse on busy->idle transition only.
3. Tried boot masking `ITC` to `0x00` at machine reset so firmware owns enable timing.

## Observed result (three cycles)

- No sampled `EI` opcode.
- No sampled `iff1=true` in `cpu-trace.jsonl`.
- M5A still reached; M6 still absent.
- Loop signatures unchanged (same PC distribution and same voice repeat interval).

## Current conclusion

- Line choice remains INT0 (trace-backed), but interrupt acceptance is still blocked by firmware state (`DI` loop or pre-EI path).
- No evidence yet that switching to INT1/INT2 would be source-correct.
