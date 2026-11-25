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

- Native asynchronous ATR/APDU transport and application-specific
  stored-value programming remain unqualified. A bounded one-way test
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
