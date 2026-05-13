# Voiceware boot path (port 0x0061)

## Firmware references

- **`VOICE_SYNTHESIS_CODE_ADDR`** = **0x61** (**`VOICEWR`**).
- **`HW_CNTL_PORT`** = **0x40**: **`VOICE_SYNTHESIS_RESET`** bit (**0x08**); init keeps **SCLK** (**0x80**).
- Voiceware driver sequences **`HW_CNTL_PORT`** with reset active/inactive around phrase output.

## Emulator behavior

- **`voice_phrase_w`**: logs **`voice_phrase`** I/O; emits milestone **`M5V`** on **first** write (not **M6** VFD).
- **`voiceware-trace.jsonl`**: enabled with **`COINLINE_TRACE_VOICEWARE=1`**; lines include **`hw_cntl_shadow`** for correlation with **0x40** latch.
- **Read** **0x61**: returns **0xFF** (open-bus style); **no** fabricated busy bit unless source proves polling semantics.

## Related

- `build/generated/voiceware-command-report.json`
