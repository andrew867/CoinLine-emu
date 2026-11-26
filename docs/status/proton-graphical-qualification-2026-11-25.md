# NLA1X09 graphical firmware qualification

NLA1X09 is a separate MTR 2.12-based graphical Multi-Pay variant (terminal
type 40, DLA 1.3). Its supplied feature configuration includes Proton, SAM,
ADSI, the 11-line display, the universal terminal role and the local
graphical Proton applet. Graphical advertising, GPM103, IOPR and the data
jack are disabled in this variant. These are firmware configuration claims,
not acceptance results.

The mapped 1 MiB flash SHA-256 is registered in
`fixtures/firmware/firmware-hashes.json`. Automatic hardware selection uses
the later Multi-Pay 1.3 preset and the 11-line expansion. The launcher keeps
its battery state separate from NLA1X08 by the full mapped-flash digest.

## Native cold run

The run in `build/firmware-acceptance/proton-install/20261125T205551.040006Z/`
started from an empty battery directory and used the firmware's normal
cabinet, keypad and handset inputs. The display reached the not-installed
prompt, entered INSTALL, completed CO detection with result 00 and proceeded
to line-number entry. With a synthetic lab identity and the local central
office, it also completed answer detection with result 00, and then displayed
the normal NCC download prompt. The download was held pending a dedicated
table-set assignment. No installed memory image or installation flag was
preloaded, and no NCC download was started.

Full NCC provisioning, INSTALL completion, warm restart, application
recognition, graphical applet behavior, SAM interactions and ADSI are
independent outstanding qualification gates. Generic asynchronous contact
transport is already covered by the native laboratory fixtures; that does
not establish a historical payment application or payment acceptance.

## Internal contact board follow-up

The separate three-socket module board is now modeled with independent
supply, reset and clock controls and a shared native ASCI0 connection.
The synthetic CPU successfully exercised T=0 and T=1 through all three
module positions and a subsequent customer-card selection. See
[native contact transport](smartcard-native-transport-2026-11-25.md).

A fresh NLA1X09 boot on the revised executable again reached the normal
not-installed prompt. Its bounded pin trace retained selector 0, with the
common module clock-control bit clear throughout the observed pre-install
interval. No module activated and no application readiness is inferred.
Evidence: `build/firmware-acceptance/proton-module-observation/20261125T220704.664640Z/`.
The full NCC set and post-install behavior still require a separate run.

## Superseding full-feature NLA1X09 variant

The newly registered 1 MiB full image has a different mapped-flash digest
from the earlier Proton-focused variant. It retains the graphical/ADSI
and Proton/SAM features and restores graphical advertising, GPM103,
IOPR and the data jack. Its feature selection is supplied configuration,
not demonstrated acceptance. New experiments use the full image; retain
the earlier digest and its evidence for comparison, with separate battery
storage.

Two blank-storage native runs reached the INSTALL prompt and entered the
CO line check. The full variant remained at `Checking C.O. connection`,
including a second handset cycle, while the focused variant previously
completed that check. Neither full-image run started an NCC download.
Evidence: `build/firmware-acceptance/nla1x09-full-install/20261126T020400.397949Z/`
and `20261126T020905.955605Z/`. This is a new regression investigation,
not evidence that the full image is ready to replace the existing package.

A controlled A/B run used the same emulator executable, blank storage,
local office, and keypad/handset input schedule for both images. The
focused image displayed `Completed: 00` and released the modem loop;
the full image remained at the CO check with the loop seized. During the
check, the focused image generated 1,136 writes to the later-board PIO A
after cycle 250,000,000 and observed the SSI detector bit. The full image
made no PIO A writes after that cycle, and did not observe the detector
bit during the same interval. No fabricated detector or memory state was
introduced. The divergence precedes NCC and warrants investigation of
the modem-task/relay sequencing in the full feature combination.
Evidence: `build/firmware-acceptance/nla1x09-pins-full/20261126T022915.972523Z/`
and `build/firmware-acceptance/nla1x09-pins-focused/20261126T022957.579944Z/`.

A repeat with native telephony transaction traces narrows this further.
Both images received the same controller handset transition bytes
`62 6E`. The focused image subsequently sent CO status query `37` and
received connected status `84`, released the internal modem relay,
and completed the dial-tone check. The full image never issued `37`
and retained R1 through the end of the test. The full image therefore
stalls before the CO status reply or SSI dial-tone detector stage.
Investigation should first establish whether the relay-sequence task
receives and dispatches the communications-suspended event; no missing
CO voltage or detector response has been established at this boundary.
Evidence: `build/firmware-acceptance/nla1x09-tp-full/20261126T024423.170982Z/`
and `build/firmware-acceptance/nla1x09-tp-focused/20261126T024456.422902Z/`.

Bounded, read-only CPU observations exposed the task dispatch and return
registers. The relay start
signal remained pending while the higher-priority service task repeatedly
returned a nonzero value in the scheduler's return register (`HL=AEAE`).
All 997 sampled returns from that task had the same value; its input
signals were clear. Inspection of the mapped instruction sequence
confirmed that the nominal zero-return path reused a nonzero register
byte. The resulting repeated dispatch starves the pending relay task.
The evidence locates the fault in the firmware image, not the CO
line or emulator's SSI detector. A corrected image is required for the
next cold INSTALL. No CPU instruction, memory value or scheduler result
was overridden. Evidence:
`build/firmware-acceptance/nla1x09-co-register-watch/20261126T030534.818592Z/`.

A full-feature candidate received for validation has mapped-image SHA-256
`187b47bac50be9fb195dac686a938ffbf837bf7435727be41592a8619b98a22f`.
An unmodified emulator started it with blank battery storage and an
isolated NCC endpoint. The CO check and answer-supervision check both
displayed `Completed: 00`; the internal modem relay released after each
check. The controller received normal hook transitions, the modem
outpulsed the configured decimal loopback number, and the local office
provided answer supervision. The firmware reached `Press * to start NCC
download` and the session exited normally without requesting a download.
This confirms the corrected firmware resolves the original CO-check
blocker. Full NCC transfer and subsequent INSTALL remain to be observed.
Evidence: `build/firmware-acceptance/nla1x09-service-return-fix/20261126T031230.374819Z/`.

A second cold run of that digest with the same bounded read-watch setup
observed the relay task progress through three dispatches, with the CO
check advancing through its successive states. It did not observe the
earlier repeated service-task return pattern. The display again reached
`Completed: 00`. Evidence:
`build/firmware-acceptance/nla1x09-co-corrected-register-watch/20261126T031828.600347Z/`.

The current full image, SHA-256
`8c7aa5129b2c752e2f45d8de578432cdf0fcf4b5c8f3c3671627b994241dc38e`,
also independently passed both cold pre-NCC checks from blank battery storage.
Both `Completed: 00` screenshots matched the earlier candidate byte for byte;
the local office observed real answer supervision and relay release. The
terminal reached the NCC-ready prompt and exited without a download attempt.
This digest is the current full-image acceptance target. Evidence:
`build/firmware-acceptance/nla1x09-current-full-cold/20261126T033538.139211Z/`.

Two cold INSTALL attempts connected to a separate isolated type-40 host
with an 18-record assignment. Both passed CO and answer supervision. The
terminal sent MT36; the host ACKed, sent MT20, and received the terminal
ACK. The native modem trace independently decodes the 19-byte MT20 with
valid CRC and eight application bytes. The terminal then entered its
closing state before accepting the host MT13; no MT44 or table delivery
occurred. Both attempts displayed `Failed: 24`.

Read-only observations of task execution and saved device state show the
clock handler was reached. The one-byte packet count was read together
with its adjacent byte, producing a nonzero high byte in the remaining
message count after all eight clock-message bytes were consumed. The
terminal reported invalid message type (error 9) while attempting to
process data outside that valid payload. This explains the clear without
implicating host CRC, timing or table contents. No response or memory
change was made to disguise the result. The next attempt requires a
corrected candidate image; the isolated assignment is preserved.
Evidence: `build/firmware-acceptance/nla1x09-full-isolated-install/20261126T035206.324964Z/`
and `build/firmware-acceptance/nla1x09-full-ncc-trace/20261126T040248.230501Z/`.

The corrected full-feature candidate SHA-256 is
`7f59977b8e25a790d7cd7d7fd02bb839dd5bead4fd33a6cf373d8ae6f06f2d55`.
A cold native INSTALL again passed both line checks. This time the clock
dialogue continued through MT13 and MT44. The independent native CRC-checked
trace and isolated host both confirm all 18 application acknowledgements:
21, 26, 31, 62, 50, 73, 92, 93, 72, 134, 135, 136, 30, 33, 49, 131, 132,
133. The terminal acknowledged the final end-of-data message and alarm
reply, cleared the connection normally and displayed `Completed: 02`.
The keypad and magnetic-reader checks advanced to the optional SAM socket
menu. Testing an empty socket left the display at `Please wait` through
more than 100 emulated seconds; a quit key did not return it to the menu.
A normal shutdown preserved the terminal-written battery state; warm restart
still displayed `Not installed`. The full INSTALL remains incomplete.
No fabricated module response, terminal memory patch or host timing change
was introduced. Evidence:
`build/firmware-acceptance/nla1x09-corrected-ncc-install/20261126T041328.461310Z/`.

A second INSTALL from the previous run's normally flushed, incomplete
battery state repeated both line checks and all 18 table acknowledgements.
The three optional SAM menu entries could be traversed with `*`; `#`
returned normally to the customer-contact test. Inserting the existing
synchronous test fixture produced native contact selector, supply, clock
and reset transitions but no asynchronous ATR. The display remained at
`Please wait` after more than 60 emulated seconds and after card removal.
The installed feature combination expects an asynchronous contact in this
maintenance test. A subsequent cold trial will use the generic asynchronous
read-only fixture rather than a historical card application.
Evidence: `build/firmware-acceptance/nla1x09-warm-partial-restart/20261126T042541.821099Z/`.

A third cold attempt used the ordinary generic asynchronous T=0 read-only
fixture with ATR `3B 00`. It again passed CO, answer, all 18 download records,
keypad and magnetic reader, and normally quit the unpopulated SAM menu.
At the customer-contact prompt, the native contact serial device activated
and transmitted both ATR characters. The terminal then shut down the card
clock and reset outputs; within the six-second post-insertion interval its
display returned to `Not installed`. No application command or transaction
was involved. The cause of the apparent restart remains unresolved; full
INSTALL and in-service acceptance are not established for this digest.
Evidence: `build/firmware-acceptance/nla1x09-full-async-install/20261126T044125.177678Z/`.

The next corrected candidate, SHA-256
`e2250aae2755b288f40f75c8b3e60e1bc33a55c8fbc30813fef7f6d56bb7aafc`,
was independently started from blank battery storage. CO and answer checks
again returned 00, all 18 assigned NCC records were acknowledged, and the
keypad and magnetic-reader tests passed. Three generic read-only asynchronous
module fixtures were configured. Selecting the optional socket 01 test
activated the contact serial transmitter; two ATR characters were sent
before the display returned to `Not installed` within the six-second
post-selection interval. No historical application, card authorization,
or transaction was involved. The separate customer-contact test was not
reached in this run. The cause of this second apparent restart remains open.
Evidence: `build/firmware-acceptance/nla1x09-corrected-contact-install/20261126T051137.390007Z/`.

The same digest repeated all 18 table ACKs in a separate cold run, normally
quit the optional module menu, and exercised the customer-contact fixture
with a valid generic T=0 ATR containing one historical byte. Bounded,
read-only CPU observations captured execution of the reset path eight
video frames after the physical insertion. The instrumented ATR parser and
completion points were not reached; the card serial device had transmitted
only the first two characters. This establishes a genuine early reset,
rather than a display-only transition or payment application rejection.
Evidence: `build/firmware-acceptance/nla1x09-contact-readwatch/20261126T052745.147574Z/`.

A second bounded trace captured the actual UART receive path. The contact
initialization and receive-enable routines executed, but the receive and
error callback addresses remained zero. The first ATR character, `3B`,
arrived with receive-ready status and no reported UART error. The serial
dispatch entered the common indirect-call path with target zero and then
executed the reset vector, all in the same video frame. No ATR parser or
application response was involved. This locates the remaining fault in
the terminal's callback initialization rather than the emulated contact
clock, card selection or serial framing. No callback, register, memory,
or device response was modified by the observation.
Evidence: `build/firmware-acceptance/nla1x09-contact-rx-readwatch/20261126T055118.206108Z/`.

A further corrected image, SHA-256
`b625f7a13fb8a85ced8d8a5bff1321ba97b0a7e5c9948e40894d1b9a1445653e`,
passed the cold CO and answer-supervision checks, all 18 NCC records, the
keypad and magnetic reader. After normally quitting the optional module
menu, the customer generic asynchronous test produced `Smart Card Test
Okay`. The read-only trace independently verified nonzero receive, error
and protocol-state addresses before RX enable, actual first ATR reception,
and entry into the expected contact parser without reset. No payment
application or authentication was involved.

The same instance completed the coin calibration and escrow/refund test
using two nickels and a dime after a normal retry, then displayed `Install
is complete. Close terminal now`. The normally saved independent coin
file recorded 20 cents refunded and zero escrow. Closing the cabinet
blanked the display. Separate warm trials from the same normally
flushed firmware-written storage remained blank, including with the
optional module fixtures absent. Physical hook transitions occurred but
no display response or normal telephony initialization was observed.
Startup CPU sampling shows continued instruction execution and graphical
key scans, while the normal application scheduler watch points did not
execute. Cold INSTALL completion is qualified; in-service startup and
warm service persistence remain unresolved. The persistent flash file
was byte-identical across the cold and warm trials.
Evidence: `build/firmware-acceptance/nla1x09-contact-initializer-fix/20261126T060420.601313Z/`,
`build/firmware-acceptance/nla1x09-full-warm-service/20261126T061729.623190Z/`,
`build/firmware-acceptance/nla1x09-warm-no-modules/20261126T061942.269431Z/`,
and `build/firmware-acceptance/nla1x09-warm-cpu-sample/20261126T062331.317777Z/`.

## Later-board common SRAM and repeat warm INSTALL

The warm loader failure was reproduced as a physical bus mapping error.
The transition from latch F9 to 07 changed the backing slice at the common
SRAM address, so the CPU fetched erased bytes rather than continuing the
RAM-resident transfer. The later-board model now keeps common SRAM fixed
across ROM selections and handles upper-window SRAM selection separately.
No bytes, checksum tables, or installation flags are copied or synthesized.
The same saved flash and battery state can now transfer into the main
application; the two window remaps and the linear RAM bounds are covered by
the native regression test.

Two fresh, blank-battery INSTALL trials on the corrected model passed both
line checks, all 18 NCC records, keypad, magnetic reader, and the normal
coin calibration and escrow checks. Both reached `Install is complete.
Close terminal now`. The optional module checks in the first trial reported
`SAM Socket Not Resp`; the customer generic contact test reported failure.
The second trial skipped the optional module tests; its customer generic
contact test also reported failure without restarting. Neither result
supersedes the earlier successful contact evidence for this image.

The first trial retained an installed marker but failed the terminal's own
memory checks on warm startup and consequently restricted service. The
second trial retained valid memory-check markers and its installed tables.
It displayed `Please lift receiver` in a genuine screenshot, but then
restarted repeatedly at approximately 3.5-second intervals. Read-only
execution observations reproduced the reset during a runtime application
reinitialization sequence. Stable warm service, native module application
behavior and another successful generic customer contact check on the
corrected memory model remain outstanding. No persistent memory or runtime
CPU state was altered during this diagnosis.

Evidence:
`build/firmware-acceptance/nla1x09-common-sram-cold/20261126T064016.543674Z/`,
`build/firmware-acceptance/nla1x09-common-sram-cold-customer/20261126T065517.311843Z/`,
`build/firmware-acceptance/nla1x09-common-sram-warm-customer/20261126T070831.640594Z/warm-10s.png`,
and `build/firmware-acceptance/nla1x09-warm-buffer-watch/20261126T071500.921474Z/`.

The corrected emulator passed its full available CTest suite (65 passed,
two ROM-dependent tests skipped), all 46 Python fixture tests, and a native
Windows build. This verifies the emulator regressions independently of the
remaining image-specific service qualification.

## Corrected full image: cold and warm service qualification

SHA-256 `0974cea06aaa2f83af007a4b0785929774be362770ff9813c79e3dc8e7a58526`
was independently installed on the corrected common-SRAM model, starting
with a blank battery directory. The two line checks returned 00, the
terminal acknowledged all 18 assigned records and cleared the dialogue
normally. The keypad and magnetic-reader tests passed. The
generic asynchronous customer contact produced `Smart Card Test Okay`,
using an unfunded contact-only fixture. The optional internal module tests
were skipped. Coin calibration required one normal retry; escrow, refund
and collection checks then reached `Install is complete. Close terminal
now`.

After closing and locking the cabinet the display showed `Please lift
receiver`; the named 3, 6, 12 and 30 second snapshots were identical.
The normal emulator exit saved the firmware-written battery, including
both intact memory-check markers. A separate restart of the same image
using its own saved battery repeated the idle display at 5, 10, 15, 20
and 30 emulated seconds. Actual handset pickup advanced through the card
prompt to `Deposit coin, insert card or dial number`; handset replacement
restored `Please lift receiver`, still present after 15 further emulated
seconds. Bounded read-only observation recorded only the normal startup
reset, not the previously recurring runtime reset. The independent warm
session exited normally. Both memory-check markers remained intact; the
persisted flash file had the same digest before and after the warm test.

Cold installation, retained service across restart, and the generic
customer-contact diagnostic are qualified for this exact digest. Optional
internal module diagnostics, application-level purse transactions and
end-to-end remote firmware transfer remain independent gates. No terminal
memory contents or validity flags were supplied or modified by the bench.

Evidence:
`build/firmware-acceptance/nla1x09-queue-corrected-cold/20261126T071808.450976Z/`
and `build/firmware-acceptance/nla1x09-queue-corrected-warm/20261126T073042.309922Z/`.

On a separate copy of its installed state, Craft item 778 reached the
internal socket diagnostic and selected socket 01. Three generic contact
fixtures were configured, with no issuer application. The pin trace records
two activations of physical selector 0 and three transmitted ATR characters
per activation. The display remained at `Please wait` for more than 75
emulated seconds. A second instrumented trial recorded the terminal's
contact-completion and menu-result routines executing, but the display
still remained pending at 20 seconds. Both trials returned to the normal
idle display when the cabinet was closed. This is not a successful module
self-test; the remaining native menu and completion sequencing requires
qualification independently of the synthetic socket transport tests.

Evidence:
`build/firmware-acceptance/nla1x09-queue-sam-craft/20261126T073413.349187Z/`
and `build/firmware-acceptance/nla1x09-queue-sam-readwatch/20261126T073806.855289Z/`.

The subsequent candidate, SHA-256
`af90b90ae1e728dc703a135ed83a91ecb7d62c463f43a3b19feeeec4c7598a90`,
completed a further blank-storage INSTALL. The terminal acknowledged all
18 records and cleared normally, passed keypad and magnetic-reader
checks, displayed `SAM Socket Okay` for menu selections 01, 02 and 03,
and displayed `Smart Card Test Okay` for the independent customer fixture.
The first calibration insertion preceded the actual deposit prompt; the
normal retry completed the coin checks. Normal cabinet closure and 30
seconds of idle service followed, with intact saved memory-check markers.

A separate warm run from that image's own saved state also retained idle
service, responded to a physical hook cycle and returned to service after
the internal contact diagnostic. Only physical internal slot 0 was
populated in that run. All three menu selections nevertheless displayed
`SAM Socket Okay`, and every recorded active contact pin observation
selected physical position 0. Therefore the menu completion is qualified,
but independent physical selection by this firmware remains unqualified;
the earlier synthetic three-position transport tests are distinct.

Read-only saved-state inspection also found a calendar discrepancy. The
retained clock-sync record contained 2026-11-26 07:44:27, while the
independently decoded terminal calendar at shutdown was 2031-10-28
03:28:46. No time-zone interpretation can account for that difference.
Timed code-download qualification for this candidate is held pending a
correct observed calendar. No clock, battery, flash or application state
has been edited to work around these results.

Evidence:
`build/firmware-acceptance/nla1x09-contact-completion-cold/20261126T074230.238305Z/`
and `build/firmware-acceptance/nla1x09-contact-completion-warm-slot0/20261126T075639.987738Z/`.

## Independent physical contact and calendar qualification

SHA-256 `996a8ac5e81daea2c3618792a15ccd5d0571a9d6ee62c5b541bc66bc27f2fb1f`
completed another blank-storage INSTALL. The terminal acknowledged all 18
records, passed keypad and magnetic-reader checks, reported `SAM Socket
Okay` for each of its three menu entries, and reported `Smart Card Test
Okay` for the separate customer contact. The native pin trace independently
observed each corresponding physical selector 0, 1 and 2 with its own
reset and clock control and actual serial activation. Twelve generic ATR
characters were transmitted in total across the four contacts. Coin
calibration was retried normally, and the terminal displayed `Install is
complete. Close terminal now`. After cabinet closure it remained at
`Please lift receiver` through the 30-second idle check.

The retained MT20 clock-sync record encoded 2026-11-26 08:08:43. Native
live observations independently decoded terminal calendar and epoch
2026-11-26 08:08:54 after synchronization, and 08:19:27 at the end of the
cold run. Both saved memory-check markers were intact. The separate warm
run used only the firmware-written state, remained in service through the
30-second idle and physical hook cycle, and then opened Craft 778 with
only physical internal slot 1 populated. Menu 01 and 03 returned `SAM
Socket Not Resp`; menu 02 returned `SAM Socket Okay`. Only the selected
populated slot produced an active contact and ATR. Normal cabinet closure
returned to idle; the warm calendar advanced to 08:21:30 and both markers
remained intact. The emulator process exited normally.

This exact image qualifies cold INSTALL, retained warm service, clock
synchronization and ordinary generic customer and distinct internal
contact diagnostics, including a negative fixture control. The contact
fixtures contain no historical issuer application or authorization state.
Historical card scheme recognition, spending, and remote image transfer
remain separate qualification gates.

Evidence:
`build/firmware-acceptance/nla1x09-selector-clock-cold/20261126T080633.216501Z/`
and `build/firmware-acceptance/nla1x09-selector-clock-warm-slot1/20261126T082024.440124Z/`.

The [independent simulated-value purse](../lab-purse.md) implements its
own durable local ledger and invented application state. It supplies no
historical SAM association or native card authorization.

## Full demo table-set download teardown, 2026-11-26

Two operator-observed graphical INSTALL attempts reported status 23 after
downloading the expanded 43-record table set. Independent host session
records show all 43 distinct application acknowledgements in each attempt,
with no missing record, retry or invalid record. Both terminal dialogues
completed the end-of-data and link ACK exchange and sent their own clear
packet. The first stored host closure timestamp followed receipt of that
clear by 15.307 ms. This is a host timestamp, not a native carrier timing
measurement. Status 23 indicates loss of established modem carrier.

Earlier successful 18-record installs used a different 36-byte MT33
record, whose carrier-loss timeout monitoring was disabled. The current
full configuration enables the monitor and uses different debounce
settings. The normal terminal-clear completion also includes a native
post-transmit delay. A retained successful native trace recorded TCP EOF
at emulated 753.866800 s, the subsequent raw detector read at 753.873800 s,
and relay release at 754.072661 s (205.861 ms after EOF). It tolerated this
ordering with the earlier configuration.

This supports, but does not yet prove, a close-timing interaction with the
enabled carrier-loss monitor rather than a bad downloaded table. The
recommended host control is to complete protocol handling at the terminal
clear, allow the terminal to release its modem and close the transport
normally, and retain a bounded cleanup timeout for abandoned peers. This
does not require changing the table, suppressing an actual carrier event,
or fabricating an acknowledgement. A shorter isolated native control and
host integration regression are needed before declaring the issue fixed.
