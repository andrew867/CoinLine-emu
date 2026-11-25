# Graphical render diagnostics

- The portable launcher explicitly disables the diagnostic defaults and
  does not set CPU, modem or graphical bus trace flags. A package process
  inspection confirmed those settings. One Craft trace helper could still
  create a small diagnostic file without an active sink; that fallback now
  checks for a requested boot or keypad trace first.
- A subsequent quiet native NLA1X05 restart and keypad/Craft-entry exercise
  created no JSONL trace files and exited normally. The standalone Craft
  menu was not entered in this particular run. The separate acceptance
  runs establish the card-reader behavior.
- The graphical renderer checks the packed visible page and optical state
  instead of allocating and expanding all 11,088 dots on every unchanged
  frame. Changed frames retain the original dot geometry, colour and
  brightness. A native bus test matched all 11,088 pixels and verified all
  four pages and twelve physical inputs after the change.
- A local GDI benchmark ran the same synthetic display program for 20
  emulated seconds with fixed frame skip and throttling disabled. The two
  baseline wall durations were 10.44 and 10.28 seconds; the two revised
  durations were 13.05 and 11.40 seconds. The busy workstation was not an
  isolated benchmark environment. These runs do not establish a speedup
  from the cache change, nor do they reproduce sustained sub-real-time
  execution on the synthetic scene. The Direct3D comparison failed to
  initialize successfully in the bench environment, so GDI remains the
  default Windows renderer.
- The revised code does not change the processor clock, peripheral timing,
  firmware pacing or display refresh rate. Whole-terminal frame rate and
  audio performance require a separate measurement with a representative
  installed program and a stable host workload.

## Subsequent hardware-clock qualification

The live graphical voice-service trace identified another, separate problem:
the terminal sampled its analogue detector every 2.5 ms with the generic
12.288 MHz oscillator. The graphical profile and V1.3 control-board preset
now select 24.576 MHz. A second actual firmware call measured the intended
1.25 ms sample interval and successfully produced its own A response to the
unaltered CAS waveform. This establishes the emulated timing correction,
not a benchmark of Windows host frame rate. See the
[ADSI qualification](adsi-interop-2026-11-25.md).

Subsequent whole-terminal runs reported average emulation speeds of 87.43%
with diagnostic tracing, 92.58% with quiet diagnostics, 94.87% with quiet
diagnostics and a reduced idle serial polling rate, and 96.19% in the final
carrier-ordering trial. These are observations of separate live runs with
different host workloads and timing options, not a controlled performance
comparison. The reduced idle poll rate avoids inspecting UART registers
when neither modem nor ADSI reception is active; it does not reduce the
serial timing resolution while a transfer is active. Real-time headroom
remains important for an externally timed voice service.
