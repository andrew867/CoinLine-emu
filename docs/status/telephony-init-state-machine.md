# Telephony Init State Machine (reference program revision Byte-Level Model)

## Transport

- Single-byte message: `[opcode]`.
- Variable-length message: `[msg_type][len][data...][chk]`.
- `len` includes `msg_type + len + data + chk`.
- `data_len = len - 3`.
- Checksum verification: `(msg_type + len + sum(data)) & 0xFF == chk`.

## CP -> TP opcodes used in init path

- `0x30` QUERY_HOOK_SWITCH_STATE
- `0x33` QUERY_PWR_INTERRUPT_STATUS
- `0x38` QUERY_ERROR_REPORT
- `0x31` QUERY_TELEPHONY_STATUS
- `0x3D` SEIZE_HOOK_SWITCH_RELAY
- `0x34` QUERY_VERSION_NUMBER
- `0x35` CLEAR_TELEPHONY_STATUS
- `0x39` CLEAR_ERROR_REPORT
- `0xC0` TELEPHONY_CONFIG (variable frame, expected `len=0x7E`)

## TP -> CP responses used in init path

- `0x70` POWER_ON_RESET
- `0x72` POWER_ON_ACK
- `0x6C` ON_HOOK_STATE / `0x6E` OFF_HOOK_STATE
- `0x7A` VALID_POWER_INTERRUPTION / `0x7C` POWER_FAILURE
- `0x74` TELEPHONY_CONFIG_ACK
- `0xC0` TELEPHONY_STATUS (`len=0x08`, 5 data bytes)
- `0xC2` TELEPHONY_VERSION_NUMBER (`len=0x0E`, 11 data bytes)
- `0xC4` TELEPHONY_ERROR_REPORT (`len=0x04`, 1 data byte)

## Expected nominal boot query block

- CP sends in order: `0x30`, `0x33`, `0x38`, `0x31`.
- TP replies: `0x6C/0x6E`, `0x7A/0x7C`, `0xC4` frame, `0xC0` frame.
- CP post-init queries: `0x3D`, `0x34`, optional `0x35`, optional config `0xC0 len=0x7E`.
- TP config success reply: `0x74`.

## Emulator implementation status (current)

- Implemented in `src/mame/coinline/millennium_state.cpp`:
  - Query handling for `0x30`, `0x31`, `0x33`, `0x34`, `0x35`, `0x38`, `0x39`, `0x3D`, and config `0xC0 len=0x7E` ACK with `0x74`.
  - Variable-frame checksum generation for `0xC0`, `0xC2`, `0xC4` responses.
  - CSI/O-side frame decode validation path for M7B evidence (`telephony_note_csio_rx_byte`).
- Current default boot byte injection path remains `0x72` (ACK path).

## Milestone status snapshot

Source: `docs/status/boot-milestone-status.json`.

- Highest reached: `M7B`.
- `M7A_TELEPHONY_ACK`: true
- `M7B_TELEPHONY_RX_PATH`: true
- `M7C_TELEPHONY_READY`: false
- Active blocker: service/display progression after telephony-ready conditions.
