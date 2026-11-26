# Native NCC session replay

The shared CoinLine and CoinLine-emu replay fixture is a complete, captured
bidirectional lab dialogue, not a session reconstructed by either encoder.

- `native-session.tsv`: ASCII, LF-terminated TSV with header `direction<TAB>hex`.
  Each row is one complete observed frame, from STX through ETX, in observation
  order. `T` denotes terminal to host, `H` host to terminal. Hex is uppercase
  with no spaces.
- `native-session.tsv.sha256`: lowercase SHA-256 followed by two spaces and
  `native-session.tsv`, LF-terminated. Both repositories must contain identical
  fixture bytes and report the same digest.

Use an isolated, synthetic lab identity and a complete routine, clock or alarm
dialogue. Review the complete capture before publication; exclude credentials,
payment records, private identifiers and operational data. Do not reconstruct
missing bytes or alter captured frames to manufacture a passing fixture.

`test_modem_ncc_capture` checks the digest, frame boundaries, CRC, exact
decode/encode equivalence, dialogue order and directional stream reassembly
with several TCP read sizes. A missing pair of files is an explicit CTest skip;
a partial pair, invalid digest or malformed dialogue is a failure.

Status: the real shared fixture is pending. No complete reviewed capture has
been accepted yet. Generated protocol vectors are tested separately and are
not evidence of capture parity.
