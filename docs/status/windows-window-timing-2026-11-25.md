# Windows window and artwork timing qualification

The native Windows OSD creates its window on the emulation thread. Its
default mouse-driven move/size operation enters a nested Win32 message loop
and explicitly pauses the running machine on `WM_ENTERSIZEMOVE`. This can
interrupt externally timed calls even when the emulated CPU and peripheral
clocks are correct. Removing only the explicit pause would not remove the
blocking message loop.

The CoinLine-specific overlay replaces ordinary mouse title-bar moves and
border resizes with nonmodal capture and position tracking. Input from the
captured frame is not forwarded as front-panel clicks. Renderer submissions
and paint redraws are deferred while the frame is captured, then refreshed
on release or cancellation. Aspect-ratio handling and minimum bounds remain
in use. Explicit MAME pauses, debugger operations, attached windows, other
machines and native system-menu operations retain upstream behavior. Native
desktop snap behavior is not reproduced by the custom mouse drag.

The machine scheduler, CPU clock, peripheral timers and firmware are
unchanged. This does not move the entire renderer to another thread; normal
draws still execute synchronously on the emulation thread.

## Native observations

A Win32 regression posts targeted nonclient messages to its own window,
checks actual mouse capture and window bounds, and samples MAME's emulated
machine clock over measured wall-clock intervals. It rejects entry into the
native modal move/size loop. These are measured clock-progress ratios from
short intervals, not frame rates or an audio-continuity measurement.

| Test | Idle | Caption drag | Border drag | Recovery |
| --- | ---: | ---: | ---: | ---: |
| Synthetic CPU, GDI, max frameskip 4 | 35.4% | 92.7% | Not completed | Not completed |
| Synthetic CPU, OpenGL, max frameskip 4 | 96.9% | 96.3% | 98.5% | 96.1% |
| Real graphical firmware, audio enabled, GDI, max frameskip 4 | 70.3% | 83.3% | 95.5% | 89.0% |
| Real graphical firmware, audio enabled, GDI, max frameskip 11 | 82.8% | 82.7% | 92.8% | 92.0% |
| Real graphical firmware, audio enabled, final native executable, OpenGL | 89.8% | 92.3% | 99.2% | 89.2% |

The initial GDI test demonstrated clock progress while redraws were
deferred, but its resize geometry exceeded the monitor's aspect bounds.
The independent free-aspect OpenGL test subsequently verified a caption
move of 40 by 30 pixels and a shrink of 60 by 80 pixels. Capture was released
and the native process exited cleanly. Fixed-aspect graphical tests exercised
capture and release even when the existing layout could not shrink on the
available work area.

These runs were not isolated host-load benchmarks. The first synthetic GDI
run preceded the successful remote service offload. Later real-firmware
comparison runs used the same program and nominal renderer options except
the stated renderer and maximum automatic frameskip. The native Direct3D
test failed during renderer initialization on this display configuration.

The portable Windows launcher and artwork acceptance harness now choose
OpenGL by default. GDI remains selectable with a maximum automatic frameskip
of 11 to shed expensive artwork frames instead of slowing the machine. The
normal OpenGL maximum remains 4. The acceptance harness can suppress
periodic PNG captures and use an independently managed CO listener.

Evidence remains in `build/firmware-acceptance/window-drag-probe/`:
`20261125T145915.225411Z`, `20261125T150252.776927Z`,
`20261125T150402.942270Z`, `20261125T150429.150801Z`,
`20261125T150512.984212Z` and `20261125T150836.697502Z`.
The last run used the rebuilt native executable with SHA-256
`7061e4bcd23ed083dac15123f9b00a6c2d195caf0099435e30cb518510315726`.

## Remote CO timing crosscheck

After the dedicated voice service and CO were moved to a separate host,
the same final executable ran an installed graphical firmware image with
audio enabled and OpenGL artwork. Automatic PNG captures were disabled;
two explicit captures were taken outside the active call. The remote CO
observed the actual handset and outpulse, answered the exact local route,
and carried the terminal's genuine A and D1. The native receiver decoded
all eight CONNECT bytes without framing errors. Exposed carrier release to
commanded D onset was 858.586 ms. The independent remote wire capture
classified completed first D about 1,160 ms after the first CONNECT burst
ended, at 20 ms packet resolution. A retry overlapped D1, and the service
closed without sending the welcome menu. A PCM underrun was traced during
the retry, not the first CONNECT. The whole-run MAME average was 75.25%.

A headless control with the same starting battery and audio enabled used
no artwork or screenshots. Its initial jack connection closed after line
presence; a second connection recovered genuine handset and relay events.
The actual call received CAS, but the firmware did not poll the detector
during that interval and sent no A. The independent wire capture likewise
found no uplink A or other detected tones and showed server voice fallback.
The terminal subsequently hung up and the CO closed the SIP dialog normally.
The whole-run average was 38.36%.
The differing startup connection and host pace prevent a controlled
renderer-only speed comparison. Removing the GUI alone was not sufficient
to qualify the live call. No service deadlines or firmware state were
changed.

Runs: `build/firmware-acceptance/remote-adsi-gui/20261125T151145.846360Z/`
and `build/firmware-acceptance/remote-adsi-headless/20261125T151635.304862Z/`.
The remote wire captures are retained outside the public source tree.

Long-running real-time audio and the live ADSI menu remain separate gates.
