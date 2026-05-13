# Disconnect supervision emulation

## Purpose

Inject **processor→host** supervision bytes into the decode queue with deterministic timing so hang-up / billing logic matches validated terminal behavior.

## Status bytes (injectable)

| Class | Hex | Emulator handling |
| ----- | --- | ------------------- |
| `LINE_INTERRUPTION` | `0x64` | Start/restart **cutoff hold** timer when state machine eligible |
| `LINE_CONNECTION` | `0x66` | Cancel cutoff timer **if** interruption sequence active |
| `LINE_REVERSAL_0` / `_1` | `0x68` / `0x6A` | Feed reversal discriminator / wink window |
| Hook transitions / states | `0x60`–`0x6E` | Hookswitch hygiene |
| `CO_LINE_STATUS_1`–`4` | `0x80`–`0x86` | Optional composite CO encoding — **bit decode profile-specific** |

## Timer coupling

| Timer | Nominal integration behavior | Units |
| ----- | ---------------------------- | ----- |
| Cutoff hold | Starts on interruption class | **`cutoff_on_disconnect_duration`** scalar — multiply by profile tick ( **`compatibility_validation_required`** ) |
| Wink | Suppresses reversal disconnect until expiry | Often **200** ticks × **10 ms** → ~**2 s** — validate |
| Defer answer supervision | Delays reversal acceptance | **50** ticks × **10 ms** → ~**0.5 s** — validate |

On cutoff expiry → emit **`hangup_required`** / teardown trace.

## Auxiliary / grace behaviors

Profile flags:

| Flag | Effect |
| ---- | ------ |
| `grace_before_collect` | Second reversal during grace may force disconnect |
| `opr_reroute` | Interruption may cancel collect timers |

**Compatibility validation required:** enable only when board profile documents auxiliary relay behavior.

## Fixture injection

```jsonc
{ "type": "supervision_inject", "t_ms": 120.5, "hex": "0x64" }
```

## Tests

[`../test-plans/disconnect-supervision-tests.md`](../test-plans/disconnect-supervision-tests.md).

## Spec

[`../specs/disconnect-supervision-device.spec.md`](../specs/disconnect-supervision-device.spec.md).
