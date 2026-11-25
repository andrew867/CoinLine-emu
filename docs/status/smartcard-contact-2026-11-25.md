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

- Native asynchronous ATR/APDU transport and stored-value programming remain
  unqualified. The generic contact fixture is not a recognized payment card.
- Runtime magnetic-stripe authorization and rating require the test-only
  host provider and provisioned synthetic account. Record firmware response,
  local call connection, authorization, CDR and settlement separately.
- Do not infer a card payment from a successful Craft contact test or table
  acknowledgement. Test fixtures and payments must be confined to the lab;
  no real issuer credentials or production transactions are included.
