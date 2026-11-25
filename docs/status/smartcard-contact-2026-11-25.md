# Synchronous card-reader qualification

## Native card contact

- The later control board now clocks the card's supply latch, reset and
  synchronous serial address counter through its actual port signals. Its
  input data contact replaces the corresponding port H bit; the earlier
  port A/C mapping is implemented separately. No firmware storage or flags
  are modified.
- The packaged synthetic contact fixture is read-only and contains no
  funded account. Its first byte is deliberately nonuniform. The device
  tests cover supply gating, reset qualification, both clock mappings,
  MSB-first serial reads and removal.
- NLA1X08 Craft item 728 displayed **Smart Card Test Okay** on each of two
  insert/remove trials. The captured native pin trace contains eight data
  reads matching the fixture, followed by power removal. In normal call
  mode, the unfunded contact fixture was rejected. Run:
  `build/firmware-acceptance/graphic-firmware/20261125T053617.302424Z/`.
- A fresh NLA1X05 cold run acknowledged all twelve NCC records and displayed
  **Smart Card Test Okay** during INSTALL. Its pin trace again matched the
  contact fixture. Coin calibration/escrow and cabinet closure completed,
  and a separate warm launch returned to service without rerunning INSTALL.
  Run:
  `build/firmware-acceptance/runs/20261125T055431.187757Z/`.

## Remaining card application gates

- At this stage, native asynchronous ATR/APDU transport and application-specific
  stored-value programming remained unqualified. A bounded one-way test
  memory programming model has unit coverage for pulse duration, address
  range, interrupted programming and removal persistence. The native bus
  probe in `build/firmware-acceptance/smartcard-pin-probe/20261125T062620.883573Z/`
  verified a programmed bit survived physical removal and reinsertion.
  It does not
  create a recognized payment card or implement a monetary counter.
- Runtime magnetic-stripe authorization and rating require the test-only
  host provider and provisioned synthetic account. Record firmware response,
  local call connection, authorization, CDR and settlement separately.
- A warm NLA1X05 copy connected to a separate loopback-only synthetic
  authorization listener. The firmware accepted the magnetic swipe and
  displayed **Thank You / Dial the number**. Its installed screening data
  rejected the reserved test number before any quote or authorization
  request. No call was connected and the simulated balance was unchanged.
  Run: `build/firmware-acceptance/graphic-firmware/20261125T062215.858884Z/`.
- Do not infer a card payment from a successful Craft contact test or table
  acknowledgement. Test fixtures and payments must be confined to the lab;
  no real issuer credentials or production transactions are included.

## Follow-up qualification

- A subsequent isolated INSTALL received all twelve revised lab records but
  exposed an overly restrictive contact-address gate: the same later board
  may read the contact with either selector value. Removed that restriction;
  the native probe now covers both selectors and contact removal/reinsertion.
  Run: `build/firmware-acceptance/smartcard-pin-probe/20261125T064942.166739Z/`.
- The warm restart needed more than the original 120-second acceptance
  budget while alarm maintenance completed. Increased the optional swipe
  test's warm limit to 300 emulated seconds. A longer interactive warm run
  returned to service, accepted the synthetic stripe, and reached card
  validation, the host quote and an approved synthetic authorization. It
  outpulsed the reserved test destination after authorization, but the CO
  did not answer because it retained manager-call supervision when the
  modem relay released with the handset still off-hook. No settlement was
  demonstrated in this run.
  Run: `build/firmware-acceptance/graphic-firmware/20261125T064954.854171Z/`.
- The CO now receives an independent modem-relay transition. A normal
  release clears management supervision without requiring a handset loop
  release. With this corrected model, the same installed battery reached
  service, accepted the synthetic stripe, obtained a second host quote and
  approved authorization, outpulsed the local test destination, and received
  CO answer supervision. Hangup returned the terminal to service. The host
  observed no CDR or debit, including during the subsequent cover alarm
  call; the attempted standalone Craft upload did not reach its menu.
  The virtual balance is unchanged. This qualifies authorization and a
  locally connected call, not payment settlement.
  Run: `build/firmware-acceptance/graphic-firmware/20261125T065959.364003Z/`.
- The same post-call battery was later started with corrected V1.3 clock
  timing and an observation-only NCC listener. It returned to service.
  Closed-cabinet Craft access, a handset cycle, and an unlocked/open cabinet
  did not reach the Craft menu; the terminal displayed the cover-open alarm.
  Normal closure restored **Please lift receiver** and the program exited
  normally. The listener observed no quote, call detail, upload, debit or
  settlement. The prior authorization had expired, and this listener did
  not register a payment provider. No forced record or memory edit was used.
  Run: `build/firmware-acceptance/graphic-firmware/20261125T083716.597527Z/`.

- The host-side synthetic provisioner had encoded a malformed seven-digit
  Craft access field in MT31, separately from the valid five-digit lab PIN.
  The normal closed-cover mnemonic therefore could not match. On a new warm
  restart, direct physical cover-open followed by the legitimate lab PIN
  entered Craft successfully. Selecting item 732 and pressing `1` displayed
  **No call records / To continue, press \***. This is a negative result
  from the terminal itself: there was no queued CDR to upload from the
  previously connected call. It is not evidence of a failed CDR transport.
  The host provider remains absent in the observation-only listener.
  Run: `build/firmware-acceptance/graphic-firmware/20261125T084537.346262Z/`.

The synthetic acceptance provisioner now uses an independently configured
seven-digit Craft access code with the required packed terminator, instead
of deriving it from the PIN. A fixture regression verifies the field across
legacy, universal and expanded layouts and confirms no unrelated records
change when that access code changes. The correction affects future
provisioning, not the retained terminal battery or the shared deployment.

The final native contact probe repeated the timed memory and two-selector
checks with the corrected board-clock build. It passed. Run:
`build/firmware-acceptance/smartcard-pin-probe/20261125T084348.990167Z/`.

## Independent terminal operator payment follow-up

- The operator lifted the receiver, swiped the synthetic magnetic card and
  dialed a rated call. The terminal displayed Card validation in progress,
  then Card is not valid / Please hang up. The host payment transaction list
  contained no entries. The NCC history included a rate request and a later
  short dialogue whose sensitive application bytes were redacted. The host
  independently confirmed that the latter was a genuine MT2 request.
- The current host authorization path returns a decline when no connected
  provider is available; the presence of a seeded account balance does not
  establish that the provider is connected. The host retains redacted audit
  metadata for authorization messages. Follow-up requires the host to
  correlate the actual authorization attempt and expose its outcome without
  storing full card data. An authorization attempt is not a settled payment.
- The operator also observed Check orientation during normal-call smart-card
  insertion. The shipped synchronous contact-test fixture is intentionally
  unfunded and is not a recognized payment application. The native Craft
  contact-test success remains distinct from runtime application recognition.
  Follow-up is to complete asynchronous selection, ATR and T=0/T=1 transport
  on the native contact pins, qualify removal and error paths, then coordinate
  an authorized synthetic application and its separate host account.
- Payment acceptance must demonstrate actual firmware authorization, rated
  call connection, terminal call record, host reconciliation and a single
  simulated settlement. No payment should be recorded solely because a
  contact test, card lookup or preauthorization succeeds.

The subsequent [native serial contact qualification](smartcard-native-transport-2026-11-25.md)
passed the generic read-only T=0/T=1 application and synchronous restart
persistence. Historical application recognition remains a separate test.
