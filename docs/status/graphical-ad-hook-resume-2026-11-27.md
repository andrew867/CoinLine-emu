# Intermittent graphical advertisement resume after hangup

## Reproduction

The investigation used independent copies of a retained, installed graphical
terminal, with the same firmware and advertisement configuration, isolated
NCC, and the simulated local CO. The previous performance build and the
subsequent waveform build were each tested through the physical hook input.
The starting display showed the rotating graphical advertisement.

After a short pickup and hangup, the normal Please lift receiver prompt
returned, but the advertisement area became blank. On the earlier build,
one out of six hangups was still blank at the 14-second checkpoint. The
five subsequent cycles resumed graphics. On the waveform build the
first hangup reproduced the same problem: no graphics were drawn through
the 60-second checkpoint, and a second pickup and hangup resumed them.

## Discriminator

The full-address VFD I/O stream was independently replayed to four page
buffers and compared against the native screenshots. All 11,088 visible
dots matched in each of 31 screenshots from the previous build and 13
from the waveform build. The affected page had no lit dots below the
two-line prompt. The processor cleared the advertisement area and did not
send any replacement pixels during the blank interval. The optical
renderer and page model therefore reproduce the actual bus state; this
is not a dropped graphical redraw or frame-skip artifact.

Both hook transitions reached the control processor's telephony interface.
The graphical task had already been reinitialized to its on-hook state,
but a deferred off-hook notification was subsequently dispatched. The
task selected its off-hook advertising handler and then the off-hook idle
handler despite the handset already being on-hook. No further advertisement
update or pixel writes occurred until the next hook cycle. The earlier and
later executables reproduce the same behavior with the same terminal state.

The evidence identifies a terminal graphical-task ordering problem rather
than an emulator renderer failure. The on-hook initializer cancels the
advertising timers but does not discard the deferred off-hook notification;
the on-hook handler accepts that notification without rechecking the
current handset state. A following timer expiry then selects the off-hook
idle handler. The recommended correction is selective cancellation of
obsolete off-hook advertising notifications, hook-state guards on their
dispatch and timer/setup paths, and initialization of the off-hook hold
duration before arming any timer that uses it. Native qualification of an
upstream correction is pending. Do not fill the VFD from the advertisement table, force a task
signal, alter retained memory, or restart advertising in the emulator.

## Repeatable acceptance

From an installed graphical seed, keep the CO local and isolate NCC. Use
the normal `graphic_session.py` hook commands and `--trace` to collect a
baseline plus repeated off-hook/on-hook screenshots. For example:

```text
wait 600
snapshot baseline
hook off
wait 0
snapshot off-1
hook on
wait 30
snapshot on-1-early
wait 120
snapshot on-1-3s
wait 300
snapshot on-1-8s
wait 360
snapshot on-1-14s
wait 960
snapshot on-1-30s
wait 1800
snapshot on-1-60s
exit
```

The analyzer reports a failure on an otherwise valid but blank settled
bus checkpoint, separately from a visual mismatch:

```sh
python tools/acceptance/graphic_ad_replay_probe.py --run <completed-session>
```

The raw captures, snapshots and read-only task trace remain local under
`build/graphic-hook-ad-regression-20261127/`. The regression must pass
with repeated short and longer pickups and the original retained
advertising data after an upstream correction.

## Corrected candidate, retained-state regression

An isolated graphical candidate with SHA-256
`33eaa2644e20a41b7179195ee698d2e552d066d29b05377381245864dff7d9e1`
was tested on the unchanged emulator with another independent copy of the
original retained state and the same failing hook sequence. The flash seed
digest prevented reuse of the previous program image; retained battery
data was loaded normally. The advertisement was visible by the three-second
checkpoint after the first hangup and remained active through the
60-second checkpoint. A second hook cycle also passed. Independent VFD bus
replay matched all 13 screenshots without a dot mismatch. The final saved
program image retained the candidate digest.

The candidate's own saved state was then restarted without an INSTALL;
the native flash and battery loads succeeded. Six more hook cycles used
the earlier reproduction sequence with varied off-hook durations. All six
had nonblank advertising data at the three-, eight- and fourteen-second
checkpoints. The additional 31 screenshots matched the VFD bus exactly.
Across both candidate runs, all eight hook cycles passed and all 44
screenshots had zero dot mismatches.

Read-only task observations show that the on-hook handler remained selected
after hangup and the advertising update restarted in the on-hook state.
The original image and user runtime state remain untouched. The candidate
has not yet been subjected to a new cold INSTALL or the wider firmware
feature matrix; this result qualifies the specific retained-state display
regression only.
