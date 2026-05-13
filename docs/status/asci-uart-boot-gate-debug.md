# ASCI / UART boot gate (status)

## Modem profile defaults

`fixtures/board/board-profile-2line-vfd.json`: **`default_dcd`: false**, **`default_cts`: true**.

## Trace

Use **`asci-trace.jsonl`** when **`COINLINE_TRACE_ASCI=1`** to verify **`STAT0`** / **`CNTL`** evolution vs **`cpu-trace`**.

## Next if blocked

If **`STAT0`** shows perpetual **not-ready** states vs modem fixture, adjust **`millennium_modem`** **without** synthesizing **NCC** frames — only **status bit** semantics aligned with source.

## Boot tranche B1 (interrupt sidecars)

With **`COINLINE_TRACE_INTERRUPTS=1`**, the driver also enables **`interrupt-events.jsonl`**, **`vector-events.jsonl`**, and **`context-switch-events.jsonl`** in the same directory as **`COINLINE_BOOT_TRACE`**, and runs the vector probe. See **`docs/status/boot-critical-runbook.md`**. Interrupt samples add **`itc`** / **`il`**; timer samples add PRT1 and **`cntr`**; reset trace logs **`machine_reset`**.
