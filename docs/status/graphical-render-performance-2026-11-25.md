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
