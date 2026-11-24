# Voice playback device emulation

The current MAME path plays the firmware-selected sample from the active 128 KiB Voiceware ROM bank. On the earlier board, PIO B at `0x41` selects the bank with bits 1 through 3. The uPD7759 completion edge advances the next firmware segment. Both profiles produced handset audio. The OEM displayed and spoke a five-cent, one-minute quote and selects spoken cents in five-cent increments. See the [verification record](status/ncc-install-firmware-stx-handoff-2026-11-23.md).

## Purpose

Emulate the **voice playback device** used for spoken prompts and tonal sequences (dial tone overlay, attention tone, coin feedback). Validated terminal behavior drives **bank selection**, **phrase code**, **reset sequencing**, and **interrupt-driven completion**.

## I/O port / bit mapping (board profile)

| Port | Bits | Emulator action |
| ---- | ---- | ----------------- |
| `0x40` | `0x08` | Voice reset (active low): pulse low ≥ documented minimum, then high before phrase start |
| `0x42` | `[3:0]` | Merge into bank shadow; mask **`0x0F`** |
| `0x61` | `[7:0]` | Latch phrase code; arm completion timing |

**IRQ:** completion interrupt source bound in `interrupt-map.json` / profile — vector wiring **`compatibility_validation_required`**.

## Behavioral sequence

1. Drive reset low → set bank on `0x42` → drive reset high → write `0x61`.
2. Wait ≥ **10 µs** (nominal integration spacing) before enabling completion IRQ if the profile models IRQ timing.
3. Signal completion to CPU or raise **`voice_fault`** if watchdog expires.

See [`../specs/voiceware-device.spec.md`](../specs/voiceware-device.spec.md) and [`../fixtures/board/voiceware-command-map.json`](../fixtures/board/voiceware-command-map.json).

## Audio output model

Authentic ROM audio is **not** required for conformance — trace fidelity is:

- `voice_segment_start` `{bank, code, cycle}`
- `voice_segment_complete` `{cycle}` or IRQ acknowledge

Optional PCM sink for evidence bundles.

## Telephony RX conditioning

When profile marks **CO connected**, mirror host→processor bytes from [`audio-routing-emulation.md`](audio-routing-emulation.md) interleaved with phrase lifecycle.

## Boot milestones

- First write to `0x61` or reset toggle → voice subsystem alive.

## Tests

[`../test-plans/voiceware-tests.md`](../test-plans/voiceware-tests.md).

## Cross-references

- [`device-model.md`](device-model.md)
- [`io-port-map.md`](io-port-map.md)
- Repository: `docs/hardware/voice-prompt-subsystem.md`
