# NCC audit remediation, 2026-11-26

| Finding | Result |
| --- | --- |
| Provisional CRC and frame geometry | Corrected. The reflected CRC-16 uses polynomial `0xA001`, initial value zero, and covers STX through the final data byte. The CRC is transmitted low byte first. Count, maximum length and the distinct six-byte control format are validated. Independent literal reference vectors replace reliance on round-trip tests alone. |
| Pending TCP connect | Corrected. Both Windows and POSIX require a completed nonblocking connection and successful `SO_ERROR` result before marking the NCC transport up. |
| Socket receive errors | Corrected. Would-block and interrupted operations are retryable; EOF and hard read/write failures close the NCC link and drop carrier. Local tests exercise refused connections, actual traffic, would-block, EOF and reset. |
| Unreachable CSI/O responder | Removed. The running device remains on its telephony processor path. |
| Legacy host bridge | Moved the standalone TCP/sideband helper to `tests/support/` and registered its tests. The actual central-office line-jack implementation remains in the driver in `millennium_line.cpp`. |
| Shared genuine NCC capture | Pending actual fixture. A replay gate, format contract and digest check are installed. It explicitly skips while the shared capture is absent. |

Also removed the obsolete synthetic NCC hello option from the modem runtime.
The native emulator forwards only actual terminal-originated serial traffic.
An old environment setting cannot inject a hello packet.

## Validation

- Windows MINGW64 native emulator rebuilt successfully.
- CTest: 72 registered, 69 passed, zero failed, three skipped (two require
  local ROM artifacts; the other requires the shared native capture).
- Python fixture suite: 47 passed.
- Native central-office line-jack probe passed.
- Native NCC isolated warm contact established carrier, exchanged clock,
  alarm, routine and maintenance messages, sent a terminal clear and released
  the modem relay. It included an excluded maintenance credential record;
  its bytes were not accepted for a public fixture.
- The capture replay reader passed generated input, digest mismatch and
  missing-manifest checks in a separate local scratch directory. This does
  not qualify actual capture parity.

The POSIX connection implementation is not exercised by the local Windows
run; cross-platform CI must cover the corresponding implementation. A fresh
full INSTALL regression is separate from the native warm-contact check.

See [shared replay format](../../fixtures/ncc/streams/README.md) for the
remaining capture gate. Both repositories must replay identical reviewed
raw bytes and the same SHA-256 before reporting it passed. No generated or
reconstructed session is substituted for an observed capture.

## Live download and refused-connect follow-up

The graphical terminal running the audited executable completed a normal
three-record selected download with distinct MT14 acknowledgements for
MT73, MT92 and MT139, terminal clear, peer EOF and Craft `Completed 02`.
This qualifies a live table-refresh regression, not a new blank INSTALL.

A separate Craft CDR upload returned `Failed 35`, the remaining-CDR result.
The earlier successful timer upload sent two CDRs and received their MT5
receipts. In the failed Craft dialogue the terminal acknowledged the same
MT17 upload request, then sent only status, version and end-of-data records
before acknowledging the host end-of-data and clearing normally. No CDR
was transmitted in that dialogue. Terminal-side record eligibility and
retention remain under investigation; do not generate receipts for absent
records or change terminal storage.

The refused-connect retry timer was also corrected. A failed initial TCP
attempt no longer consumes the current modem dial; the minimum retry
interval is one emulated second while R1 remains seized. An established
TCP connection consumes the dial, so peer EOF or hard failure cannot
silently reopen a previously connected call. Explicit redial or a new
relay seizure starts a new dial. Loopback regression covers an initially
unavailable peer becoming available within the same seizure, no early
retry, successful deferred carrier, no automatic reconnect after peer
closure, and cancellation on relay release.
The Windows native rebuild and all 69 runnable CTest cases passed; the
same three optional artifact-dependent tests remain skipped.
