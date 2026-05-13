# Voiceware/Scheduler Source Correlation

Last updated: 2026-05-05

## Correlated points

- Voice synthesis assembler module (`VOICESYN.ASM`):
  - `_voice_synthesis_int_handler` exists and writes next/repeat phrase to `VOICE_SYNTHESIS_CODE_ADDR` (`0x61`).
  - Voiceware driver behavior describes INT0-driven segment completion (segment scheduling via interrupt completion).
  - `start_voice_synthesis_output()` toggles reset, writes bank/code, then enables INT0 (`enable_voice_synthesis_int`).
- Existing repo correlation docs identify `0x00CF..0x00E5` as `xentry_int_sub` context-save band.
- New `ei-di-events.jsonl` confirms this band is repeatedly entered with fetched `DI` and exits with `RET`.

## Answers required by bring-up task

- What source routine is PC `0x5B29`?  
  - Still unresolved to exact symbol in current workspace artifacts; observed as repeated firmware voice command site (`OUT0 0x61`).
- What routine should run after voice completion?  
  - `_voice_synthesis_int_handler` path (INT0-driven), then scheduler/task signaling in firmware support code.
- Completion mechanism expected?  
  - IRQ-driven (INT0) by source comments and interfaces; polling is not primary in source path.
- Which interrupt line?  
  - Source-backed expectation: Z180 INT0 (`ITC` bit `ITE0`).
- What enables interrupts?  
  - Firmware EI path not yet observed in new opcode event trace window; many `DI` samples were observed.
- Why EI not reached in current trace?  
  - Current evidence indicates firmware remains in repeated context-save/call band (`0x00AD -> 0x00CF -> 0x00E5`) and voice retry cadence before any sampled `EI`.
