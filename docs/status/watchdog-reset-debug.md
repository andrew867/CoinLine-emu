# Watchdog / reset debug

## Current evidence

`cpu-trace.jsonl` samples include **`pc":"0xFFFF"`** — logical top-of-memory per MMU map to SRAM physical (`z180-register-trace` correlates). **Not** treated as CPU reset without **PC=0** sustained run.

## Report

`build/generated/reset-cause-report.json`
