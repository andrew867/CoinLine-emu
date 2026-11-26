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

The [independent simulated-value purse](../lab-purse.md) implements its
own durable local ledger and invented application state. It supplies no
historical SAM association or native card authorization.
