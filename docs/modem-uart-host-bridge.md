# Modem UART and host bridge

## Firmware-facing interface

ASCI1 is the manager modem link on the verified MAME profiles. The modem samples terminal transmit data at the serial pin when the UART uses its internal clock. It reports carrier through the modem interface and delivers incoming bytes over the serial path. ASCI0 is reserved for the coin validator, including its wake, status, and denomination exchange.

Firmware SSI writes control the modem and generate the management dial digits. The CO connection check uses the independent line-jack battery, tone, hook, and polarity state. Telephony-processor commands control handset audio and voice-call digits.

## Transport options

The verified manager connection uses TCP. The firmware dials the decimal address `2130706433` for `127.0.0.1`, and the modem opens the local CoinLine connection after that outpulse sequence. The default NCC port is 4000. Set `ncc_port` in the portable launcher configuration or `COINLINE_NCC_PORT` when launching the driver directly.

On Windows and POSIX, a nonblocking connection is not established until
the socket is ready and its `SO_ERROR` query succeeds with no error. The
existing connect wait is bounded at 400 ms. Would-block or interrupted
send/receive operations are retried on subsequent polls; EOF and hard
transport errors close the link and report carrier loss. TCP connectivity
alone still does not assert modem carrier.

For MTR 2.x remote program downloads, an optional second ten-digit dialing
alias selects a separate loopback TCP code server. Set `code_server_number`
and `code_server_port` under `[line]` in the launcher configuration, or set
`COINLINE_CODE_SERVER_NUMBER` and `COINLINE_CODE_SERVER_PORT` directly.
The firmware's outpulsed number determines the route. The modem preserves
the raw serial byte stream and uses the existing carrier sequence. Unmatched
numbers do not open data sessions. See [remote firmware downloads](firmware-download-storage.md)
for the configuration and qualification plan.

A separate loopback TCP connection carries line-jack signals and handset audio to the central-office process. The portable launcher starts one office for each running profile. Local mode supplies dial tone, answer supervision, and a local test destination. With a configured SIP account, SIP mode connects outbound calls and bridges audio between the jack and the SIP peer.

The live manager path is implemented in `millennium_modem_model.cpp` and
the central-office line jack in `millennium_line.cpp`. The older raw TCP
and optional JSON side-band helper is under `tests/support/` and is linked
only into its loopback tests, not MAME. The device does not insert such
side-band bytes into NCC calls.

## Verified NCC exchanges

The lab captured clock sync, ten-table INSTALL downloads, maintenance uploads, alarms, and MT63/64 live rating. The bytes came from the firmware and the CoinLine server. The [verification record](status/ncc-install-firmware-stx-handoff-2026-11-23.md) documents the transfer ordering and acceptance. See the [launcher](../RUNNING.md) and [line-jack protocol](../tools/line-jack/README.md) for operator and integration details.

The independent NCC framing helper now uses the reflected `0xA001`
polynomial, initial value zero, no final XOR, little-endian CRC over STX
through the last data byte. Control packets have count 5, no terminal ID
or data, and six wire bytes; message packets have count 10 plus data
length, and the entire wire length is count plus one. The runtime modem
still transparently forwards the terminal's native NCC bytes.
