# Audio Routing Implementation Status

Last updated from run: `build/runs/20260505T140947-boot-critical`.

## Current State (Evidence-Backed)

- Voiceware command observed in latest 30s run: NO
  - Top I/O ports include `0x0060` (VFD) and `0x00E1` (telephony UART), but not `0x0061` (voice command).
- WAV output: not produced (latest `audio-capture-report.json` says `wav_requested=false`, `wav_present=false`)
- Audible/non-silent proof: not available in latest run artifacts

## Exact Missing Pieces

- A trace-backed audio route model (handset/earpiece/speaker/line) that changes with hook and firmware control bits.
- A dedicated audio capture run path that requests WAV output and stores it under the run folder, then computes a simple non-silent classification.

