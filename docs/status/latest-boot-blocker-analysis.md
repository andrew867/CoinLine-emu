# Latest Boot Blocker Analysis

Latest analyzed run: `build/runs/20260505T140947-boot-critical`.

The focused M7 gate moved. Firmware now reads `0x72` from the external UART path and `boot-trace.jsonl` records `M7_TELEPHONY_BOARD_RESPONDS`.

Protocol decode:

- `0x72` = `POWER_ON_ACK` (single-byte telephony opcode toward control CPU).
- `0xC0` = `TELEPHONY_STATUS`; variable-length frame length is **8** bytes for the modeled idle payload.
- `0xC2` = `TELEPHONY_VERSION_NUMBER`; variable-length frame length is **14** bytes for the modeled payload.

Implemented model: a trace-backed external UART at `0xE0-0xE7` with 16550-style init behavior plus telephony response frames for power-on/status/version/error-report queries. The reduced `uart` trace profile keeps fetch/stack/vector tracing disabled for usable 30s visible/audio runs.

Current blocker after this focused pass: `post_m7_telephony_protocol_progression`.
