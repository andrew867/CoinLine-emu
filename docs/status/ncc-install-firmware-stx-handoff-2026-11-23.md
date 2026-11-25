# Firmware INSTALL and NCC verification

## Cold boot and installation

- The MTR 1.9 demo started with erased serial EEPROM and blank SRAM. Firmware wrote Microwire words and copied table defaults into SRAM. A later boot read the persisted values.
- Both the demo and normal OEM firmware entered INSTALL after the craft code and PIN. Each passed central-office line detection and answer supervision.
- The firmware originated the NCC sessions over the ASCI1 modem path. Clock synchronization reached the install request, followed by ten application-level table acknowledgements for types 21, 22, 26, 31, 50, 62, 73, 92, 93 and 101. The largest table used continuation packets.
- During bring-up, a late transmit-complete signal rearmed the firmware's reply timer after link acknowledgement. Pacing the host acknowledgement after transmit completion allowed the transfer and cold-start alarm exchange to finish.
- Both firmware images passed keypad, Track 2, nickel, dime and quarter tests. Closing and locking the terminal after INSTALL brought it into service. A restart with the same battery image returned to **Please lift receiver**.

## Calls and Voiceware

- The lab call-screening record enabled the unrated toll-free path. The demo outpulsed all eleven digits and an answered SignalWire call played the far-end directory assistance prompt.
- A live OEM MT63 request returned an MT64 quote. For a five-cent one-minute rate the display showed the deposit amount and Voiceware announced it. The tested OEM build speaks amounts in five-cent increments.
- The Voiceware bank latch selected the appropriate ROM window. The uPD7759 played the sample and delivered its completion edge to the terminal.

Use the [operator guide](../../RUNNING.md) for the INSTALL sequence. At the initial out-of-service display, lift and replace the handset before entering CRASERV if the craft prompt does not appear.
