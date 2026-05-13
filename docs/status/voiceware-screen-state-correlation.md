# Voiceware / Screen-State Correlation

Last updated from run: `build/runs/20260505T140947-boot-critical` (30s).

## What We Can Prove From This Run

- Final VFD text indicates telephony-not-responding state:
  - `Telephony board is`
  - `not responding`
- No voiceware command write (`0x0061`) is observed in the latest 30s capture.
- No WAV output was requested or produced in this run (`audio-capture-report.json`: `wav_present=false`).

## Phrase/Tone Status (Honest)

- "doo doo dee" tone path: NOT CAPTURED in current evidence.
- "Please insert and remove your card" prompt path: NOT REACHED in current evidence.
- Phrase text claims: UNPROVEN (no source+audio correlation for this run).

## Next Required Evidence

- A run that (1) exits telephony-not-responding state, (2) triggers a real voice command (`0x0061`), and (3) produces a non-silent WAV or audible output that can be correlated to the current VFD screen.

