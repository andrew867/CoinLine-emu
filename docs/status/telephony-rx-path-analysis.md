# Telephony receive path (analysis)

This summarizes **observed on-wire / trace-backed behavior** for the telephony bring-up path (no proprietary dumps). Emulator work should match the **CSI/O inter-processor link** as the primary telephony-to-control path; the **external 16550-style block at host I/O 0xE0–0xE7** is a **separate experimental/alias** visibility path in the driver and must not be treated as proof that the real `ip_rx_buffer` consumer ran.

## Circular buffer `ip_rx_buffer`

- **Role**: FIFO between the telephony-facing link layer and higher-level decode in the terminal software (decoded in the telephony/message task, not solely by UART glue).
- **Declared**: as a circular-buffer control structure in the IPC layer (exported to both C and assembler).
- **Writers**: bytes are **assembled from the CSI/O bit stream** driven by firmware (hardware control clock / shift semantics) and **stored into the circular buffer** in the IPC interrupt path. Separate writers also push ** keypad / ADSI** injection traffic into the **same buffer**—that path is orthogonal to telephony-board responses.
- **Readers**: decoder logic **reads the buffer sequentially** (`rd_cbuffer` pattern), validating **checksum** against the additive rule for framed messages (`>= 0xC0`), and consuming **single-byte** messages immediately for opcodes `< 0xC0`.

## Status frame (opcode 0xC0)

- Firmware treats opcodes **`>= 0xC0` as framed**: length byte follows opcode, then `(length - 3)` payload octets (including framing fields), final octet additive checksum covering opcode, length, and payload bytes preceding the checksum (`length` semantics match the emulator’s `[code][len][payload…][checksum]` builder).
- **TELEPHONY_STATUS** fits this pattern (`length == 8` for the modeled idle-status payload).

## QUERY_HOOK_SWITCH (opcode 0x30)

- **`< 0xC0`** traffic is modeled as **single-byte completion** once the opcode’s first data octet arrives; the emulator must **not** answer hook query `0x30` as a **`0x30`-headed framed reply**—that would violate the IPC framing rules inferred above and stalls progression.

## `TERMFG_TELEPHONY_UP` / readiness

- Set after **successful decode** of telephony status / error-report path from the **`ip_rx_buffer` consumer**, not after an unrelated UART poll path. Exact flag storage is not resolved from artifacts in this repo; readiness is evidenced by decode success plus UI leaving the stall text (no VFD injection in the emulator).

## UART alias 0xE0–0xE7 vs CSI/O

| Path | Firmware mechanism | Emulator notes |
|------|-------------------|----------------|
| CSI/O + IPC | ISR-driven path fills `ip_rx_buffer` | `board_status_w` SCLK shifts bits; **`telephony_note_csio_rx_byte`** mirrors bytes for milestones/traces (`M7B`) |
| 0xE0–0xE7 | Separate driver model | OK for `M7A` **ACK read** via `0xE1`-style alias; **not** proof of `ip_rx_buffer` |

## Emulator bridge (bring-up)

Earlier builds reconstructed CSIO **TX** bytes from **TXS change callbacks** only. MAME’s Z180 CSIO fires **`txs_wr`** when the serial output **changes**, so repeated bits could **not** produce eight edges per byte — traces showed **`0xAA`**-style garbage versus expected **`QUERY_*`** telephony opcodes such as **`0x31`**. The driver now takes the **transmit opcode from `TRDR`** when **`shift_cnt == 7`** on an external-clock **falling edge** with **TE-only** (**`CNTR`**) — i.e. the same octet the Z180 is shifting out on the CSI/O path. Boot captures show **early CSI/O transmits starting with `0xC0`** (variable-length framed traffic), **not** a bare **`QUERY_TELEPHONY_STATUS` (`0x31`)** byte in isolation; **`0x31`** may appear later in the IPC queue. **`case 0x31`** in the modeled telephony shim therefore does not always run during the window that validates **`TELEPHONY_STATUS`** on the UART alias. After the UART path consumes a checksum-good **`0xC0`** status frame, the driver **still replays those eight octets** onto the modeled CSIO RX queue so **`telephony_note_csio_rx_byte`** can reach the **M7B** parser milestone (until IPC TX sequencing is modeled further). The **external-UART** path at **`0xE0–0xE7`** remains a separate visibility path for milestones like **M7A**. **M7C** still requires the firmware decode/UI path to advance.

## Open items

- Correlate **`ipcomm_tx_byte`** (from **`TRDR`**) with the modeled IPC transmit queue so **`TELEPHONY_STATUS`** responses can be queued from CSI/O TX alone without UART replay.
- **Runtime proof** of `TERMFG_TELEPHONY_UP` requires optional operator-supplied debug hooks (not committed here).
- **Term-flag JSONL** remains future work unless a stable physical address is supplied via environment configuration.
