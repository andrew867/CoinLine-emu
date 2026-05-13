# NIS craft/install acceptance — success record

## Summary

The NIS craft/install acceptance vector (`tools/mingw64/run-nis-craft-install-acceptance.sh`) completed with **A1–A6 true**, validation **OK**, and **WAV non-silent**.

## Firmware loading

Split flash images (`flash.bin` + `flash1.bin`, each 512 KiB) are concatenated in-device when `COINLINE_FIRMWARE_FLASH0` / `COINLINE_FIRMWARE_FLASH1` are set; the harness auto-detects these files under `../firmware/` relative to the emulator repo when present.

## Evidence

Pinned artifacts for the passing run live under `build/runs/<timestamp>-nis-craft-install-acceptance/` (see `build/evidence/nis-craft-install-acceptance/evidence-index.json`). Operator-supplied binaries are not committed.

## Gates (reference)

| Gate | Meaning |
|------|---------|
| A1 | Stable out-of-service screen; TP runtime fault from health trace, not VFD scroll artifacts |
| A2–A4 | Real MAME hook input with TP UI / CSI-O-visible hook path |
| A3 | Non-silent handset audio WAV |
| A5 | Keypad `2727378` via TP ordered digits or CP buffer model |
| A6 | CP-side craft gate (`craft_gate_accept` / `craft_code_detected`), not VFD substring alone |
