# Later-board supervision and interruption tests

The later control board has a positive watchdog-service pulse on `0x63`
bit 7. Firmware services it periodically at about 500 ms, with documented
maximum separation of one second. A complete low-to-high-to-low pulse is
observed at the emulated time; an initial high latch or a continuously high
output is not counted as a service. Optional tracing records pulse width and
separation in emulated nanoseconds, along with telephony power-indication
and link-enable transitions. Normal runs have no new watchdog deadline.

The explicitly selected `cp-only` supervision test profile uses a provisional
one-second deadline and pulses the actual Z180 reset input on expiry. It
does not reset the telephony controller, select a different program bank,
change persistent memory or drive the firmware's power-available output.
The actual supervisor threshold, pulse qualification, reset wiring and
fan-out remain unmeasured. This profile is therefore not a default physical
board claim.

Separate diagnostic inputs assert the Z180 NMI line and mark the control
rail unavailable. The NMI is an independent edge, not a write to the
telephony power indication. Rail-unavailable holds only the control CPU;
the watchdog deadline is stopped and NMI is ignored until the rail is
restored. On restoration, the CPU is reset under the provisional profile.
This models a deliberate test interruption, not physical supply voltage,
capacitance, RTC retention, reduced-power service or partial flash damage.

The native synthetic program, with normal Z180 interrupt entry at `0x0066`,
passed its NMI handler, deliberate watchdog stall, rail-unavailable and
restore tests. It generated three genuine service pulses and exactly one
watchdog reset. Evidence:
`build/firmware-acceptance/board-supervision-probe/20261125T231225.148463Z/`.
The component test establishes edge qualification, pulse width and gap
calculation independently. No firmware was modified or installation state
preloaded.

## Passive actual-firmware timing

A blank-battery NLA1X09 run with tracing but no deadline enabled recorded
128 genuine watchdog pulses. The longest gap was 1,094.361491 ms during
startup; one gap exceeded the documented one-second service deadline.
After emulated time 10 s, the largest observed gap was 502.501627 ms.
The first two telephony power indication sequences measured:

| Sequence | Low duration (ms) | Restore to link-enable (ms) |
| --- | ---: | ---: |
| 1 | 103.349202 | 61.332276 |
| 2 | 308.470215 | 69.232341 |

These are latch and model link events measured on the emulator clock,
not physical supervisor or processor power-on measurements. The first
low duration differs from the documented 150 ms software sequence.
Neither its cause nor the startup watchdog gap is yet isolated; the
profile remains disabled normally instead of extending a guessed
deadline to disguise the difference. Evidence:
`build/firmware-acceptance/board-supervision-oem/20261125T231255.214964Z/`.

### Native NMI recovery

A second blank-battery NLA1X09 session used the interrupt-only profile,
with the watchdog deadline disabled. Before the diagnostic NMI the VFD
showed the not-installed prompt. The real CPU received an NMI, and the
firmware itself subsequently asserted and released the telephony power
indication and re-established the link. Its VFD returned to the same
not-installed prompt. The saved 1 MiB flash matched the original seed
exactly, with zero changed blocks. No external power-indication output or
reset was forced by the NMI input. Evidence:
`build/firmware-acceptance/board-power-oem/20261125T231643.784786Z/`.
This is a genuine firmware restart response to a simulated NMI; installed
call recovery, physical rail disappearance and interrupted DLA recovery
remain separate gates.

## Running the diagnostics

`board_supervision_probe.py --mame <executable>` runs the isolated synthetic
bench. The interactive later-board acceptance launcher accepts
`--board-supervision-trace` for passive actual-firmware observation or
`--board-supervision-test` for the explicit provisional profile. The
`--board-power-test` profile enables the independent control NMI/rail
inputs without a watchdog deadline. Its command
file supports `power-nmi`, `control-rail down`, and `control-rail up`.
Do not use the provisional profile on a retained production state. The
documented firmware initialization sequence is at least 150 ms of low
power indication followed by 60 ms before communication; compare actual
trace events instead of replacing the firmware's delays.
