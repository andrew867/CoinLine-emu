# PC FFFF / RST38 Root Cause

## Result

The first `PC=0xFFFF` fault in `build/runs/20260505T093612-boot-critical` was caused by a corrupted stack return address. It was not a direct interrupt vector, not linear execution into erased ROM, and not a deliberate firmware branch to `0xFFFF`.

## First Fault

- The first bad control transfer was a `RET` at `PC=0x3151`.
- The return address popped from the stack was `0xFFFF`.
- The return bytes were read from logical stack `SP=0x4FC4`, translated to physical SRAM around `0xC4FC4`.
- The expected return for the preceding call path was `0x48A4`.
- The following fetch at logical `0xFFFF` translated to physical `0xC7FFF`, source `sram_128k`, byte `0xFF`, which produced the RST38 loop.

## Root Cause

The program map installed physical SRAM at `0xC0000-0xDFFFF` with lambdas that passed the range-relative MAME callback offset directly into `phys_ram_r` / `phys_ram_w`. Those helpers expect absolute physical addresses. As a result, writes to SRAM were discarded and reads returned the erased `0xFF` fill pattern. Runtime BSS, task pointers, and stack bytes in SRAM did not persist.

## Fix

`millennium_memory.cpp` now adds the physical base before calling the SRAM helpers:

```cpp
0xc0000 + offset
```

The follow-up run `build/runs/20260505T094137-boot-critical` did not produce `first-pc-ffff.json`, did not show `popped_return_ffff`, and advanced to M8. The PC `0xFFFF` / RST38 / voice replay loop is eliminated.

## Current Gate After Fix

The latest 30s run, `build/runs/20260505T094715-boot-critical`, still does not reach M6. It reaches M8, has no port `0x0060` VFD data, no `0xB3` voice repeat, and no PC `0xFFFF` recurrence. The next gate is ASCI/modem or board-status scheduling after M8, not the original memory bank fault.
