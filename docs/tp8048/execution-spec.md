# PCD3349A / 8048 TP Execution Spec

## Goal

Move TP behavior into a chip-oriented backend that executes MCS-48 style firmware at a PCD3349A-compatible clock, while preserving CP-visible behavior and retaining legacy backend A/B testing.

## Hardware facts used

- PCD3349A is an 8-bit microcontroller with 4 KiB ROM, 224 bytes RAM, 20 quasi-bidirectional I/O lines, timer/event counter, external/timer interrupts, and DTMF generator.
- Instruction set is based on MAB8048.
- DTMF-friendly oscillator is 3.58 MHz.
- Frequency generator has HGF and LGF write-only derivative registers at addresses 01H and 02H.
- Reset clears derivative registers.
- Serial I/O instructions are unavailable on this derivative, so the emulator wrapper must bridge CP/TP CSI-O at the port/callback level rather than relying on native serial instructions.

## Clocking

Default TP clock:

```text
3579545 Hz preferred, 3580000 Hz acceptable if board profile says so.
```

The wrapper must expose this in traces:

```json
{"tp_backend":"pcd3349a_8048","tp_xtal_hz":3579545}
```

## Backend shape

```text
millennium_state
  -> selected TP backend
     -> millennium_pcd3349a
        -> millennium_am8048_core
           -> behavioral TP firmware ROM
```

`millennium_state` must not directly set craft flags, display text, or CP memory. It should only:

1. Deliver qualified CP bytes to the TP backend.
2. Provide sampled TP-side input snapshots.
3. Step the TP backend by cycle/cadence.
4. Pull TP output bytes and enqueue them into the CP CSI/O receive queue.
5. Read tone intent from TP backend and route to audio model.

## Required traces

- `tp-8048-runtime-trace.jsonl`
- `tp-8048-port-trace.jsonl`
- `tp-8048-keypad-trace.jsonl`
- `tp-8048-tone-trace.jsonl`
- `tp-8048-cp-protocol-trace.jsonl`

Each trace event should include:

```json
{
  "backend":"pcd3349a_8048",
  "tp_cycle":0,
  "cp_cycle":0,
  "pc":"0x0000",
  "event":"...",
  "value":"0x00"
}
```

## Tone implementation

Phase 1 can use C++ tone synthesis controlled by the 8048 firmware's tone register writes or tone intent. The PCD3349A backend should still model the HGF/LGF register writes so trace evidence shows chip-oriented behavior.

Tone modes:

- none
- dialtone
- NIS
- DTMF digit tone

NIS target: 480 Hz + 620 Hz.
Dial tone target: 350 Hz + 440 Hz.

**Scope note:** Frequencies above model **local earpiece / progress / DTMF** intent at the TP. **Modem Bell 103-class tone pairs** for host data live on the **CP modem/ASCI** side in product documentation; do not conflate those with TP `HGF`/`LGF` writes.

## Hook switch visibility to the CP

Behavioral firmware mirrors CP-visible sequencing:

1. On debounced hook change, enqueue the **transition** response byte for on-hook or off-hook.
2. Start (or rely on) the **internal timer** so periodic **timer interrupts** run.
3. After a bounded delay counted in timer ISR passes, enqueue the **steady-state** on-hook or off-hook byte.

The wrapper must advance the 8048 core enough between CP samples that timer interrupts can run when deferred steady bytes are armed.

## Timer interrupt obligation

The integration (`millennium_pcd3349a_contract::run_until_cp_cycle`) steps the TP core in bulk against CP cycles. Firmware must execute **`STRT T`** after enabling timer interrupts so `TIMER_IRQ` fires; otherwise countdown-based deferral stalls.

## Acceptance gates

This backend exists to pass:

- OOS stable.
- Hook off/on through TP.
- NIS audio from TP tone path.
- Keypad `2727378` through TP event path.
- CP consumption of key events.
- Craft/install entry.

## Non-cheat constraints

Do not:

- insert VFD text,
- patch CP memory,
- set craft accepted flags directly,
- bypass CP/TP queues,
- emit pass evidence from emulator-only state without protocol evidence.
