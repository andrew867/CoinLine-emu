# TP DTMF Tone Spec

## Goal

- Model TP-oriented tone intent and deterministic output routing without fake UI shortcuts.

## Tone Sources

- TP tone intent (PCD3349A-side model):
  - `none`
  - `dialtone`
  - `nis`
- **Not in this spec:** **Bell 103 / modem carrier tones** used for **host data sessions** over the **ASCI/modem** path. Those are modeled separately from TP earpiece progress tones (see [`terminal_01_modem_ncc_emulator_spec.yaml`](../protocols/terminal_01_modem_ncc_emulator_spec.yaml) and [modem hardware](../../../docs/hardware/modem_hardware.md) at repo root). The PCD3349A still exposes **HGF/LGF** DTMF-related registers; host-link FSK/audio is **not** the same register block.
- CP/voice subsystem coexistence:
  - suppress local earpiece tones while voice playback is active
  - preserve mute/routing constraints from audio-route state

## Routing Rules

- Tone output active only when:
  - handset is off-hook
  - RX path is open
  - selected tone mode is not `none`
- OOS/not-responding path prefers NIS mode.
- Idle/dialing path prefers dialtone mode when not voice-active.

## Frequency Profile (Phase 1)

- Dial tone: 350 Hz + 440 Hz.
- NIS indication: 480 Hz + 620 Hz.

## A/B Behavior

- Legacy backend:
  - existing tone selection path remains baseline.
- PCD3349A backend:
  - same externally observed mode transitions with backend-specific decision logic.

## Validation

- Runtime trace must include tone-mode transitions with:
  - hook state
  - RX mute/open state
  - voice active state
  - OOS/not-responding state