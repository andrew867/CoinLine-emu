# Timer / interrupt implementation debug

## MAME Z180

Timers (**PRT**), **ASCI**, **ITC/IL**, and **IFF** are implemented inside `z180_device`. The driver does **not** duplicate internal register semantics.

## Tracing added

When enabled via environment (see `run-screenshot-capture.ps1`):

| Env | File |
|-----|------|
| `COINLINE_TRACE_INTERRUPTS=1` | `interrupt-trace.jsonl` (`iff1`, `iff2`, `im`) |
| `COINLINE_TRACE_TIMERS=1` | `timer-trace.jsonl` (`tcr`, `rldr0`, `tmdr0`) |
| `COINLINE_TRACE_ASCI=1` | `asci-trace.jsonl` (`cntla0`, `cntlb0`, `stat0`) |
| `COINLINE_TRACE_RESET=1` | `reset-trace.jsonl` (PC==0 watch — coarse) |

## Events JSON

`build/generated/timer-interrupt-events.json`
