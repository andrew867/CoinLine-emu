# VFD Boot Screen Analysis

- Reference run: `build/runs/20260505T105045-boot-critical`
- Final text (`vfd-final-text.txt`):
  - `Telephony board is  `
  - `   not responding   `
- This text is firmware-driven (real `0x0060` writes and strict M6 pass), not overlay/injected output.
- First VFD write: cycle `7473834`, PC `0x5F63`, SP `0xCF9E`, byte `0x14`.
- Current post-M6 blocker classification: `telephony_board_not_responding`.

## Observed progression

- Firmware reaches M6 and writes valid 2-line VFD output.
- Screen advances from out-of-service text into telephony-board fault message.
- No evidence of spoofed display transition; message originates from firmware command stream.

## Next diagnostic focus

- Determine what condition sets/clears firmware `TERMFG_TELEPHONY_UP`.
- Emulate minimal telephony response path needed by initialization and craft logic to clear not-responding state.
