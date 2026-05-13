# Umbrella spec: Voiceware, voice ROMs, alerter, DTMF, call-progress tones

Hardware IDs: **HW-VOIC-001**, **HW-VOIC-002**, **HW-SPK-001**, **HW-DTMF-001**, **HW-CP-001**

## Voiceware (HW-VOIC-001 / HW-VOIC-002)

- **Device:** `millennium_voiceware.cpp`; ROM region `voicew` in `millennium.cpp` (U16/U26).  
- **Ports:** `0x40`, `0x42`, `0x61` per `fixtures/board/voiceware-command-map.json`.  
- **Trace:** `voiceware-trace.jsonl`, WAV output path when enabled.  
- **Tests:** `test_voiceware_*`.  
- **Acceptance:** Phrase commands drive MAME `upd7759`-style playback — **no** silent WAV presented as success without energy detection in trace/report.

## uPD7759 core execution fidelity (**HW-VOIC-003**)

Target: **chip-faithful** playback using MAME **`upd7759_device`** and **`busy_r()`** for busy/IRQ, **immutable** voice ROM dumps, optional **post-chip** LPF/gain before handset routing.  
**Spec:** [`HW-UPD7759-VOICE-ROM-EXECUTION.spec.md`](HW-UPD7759-VOICE-ROM-EXECUTION.spec.md) · **Integration:** [`../voiceware-upd7759-core-execution.spec.md`](../voiceware-upd7759-core-execution.spec.md) · **Program:** [`../../docs/status/UPD7759-CORE-EXECUTION-PROGRAM.md`](../../docs/status/UPD7759-CORE-EXECUTION-PROGRAM.md).

## Alerter / DTMF / call progress

- **Ports:** `0x58`–`0x5B` — `millennium_audio*.cpp`, `millennium_audio_model.cpp`.  
- **Tests:** `test_alerter_*`, `test_alerter_dtmf.cpp`.  
- **Acceptance:** Cadence from scheduler — works with `-sound none` for GPIO/trace proof; WAV optional.
