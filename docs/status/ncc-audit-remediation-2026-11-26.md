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
