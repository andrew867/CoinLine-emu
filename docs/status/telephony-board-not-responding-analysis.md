# Telephony Board Not Responding Analysis

## Routine and trigger condition

- The telephony-not-responding prompt is shown when the runtime telephony-up flag is clear.
- Telephony-up is only set after init receives a telephony status event and marks telephony information as received.
- If that status path is not completed before timeout handling, firmware keeps the board-down condition and the VFD remains on `Telephony board is / not responding`.

## Port/register path observed in emulator

- External UART range is active at `0xE0-0xE7`.
- Observed traffic in latest UART run:
  - startup programming writes (`LCR`, `DLL`, `DLM`, `MCR`, `IER`)
  - one early `RBR` read and `LSR` read
  - repeated later `IER` polling reads while blocked
- Current run evidence (`build/runs/20260505T131152-boot-critical`):
  - `rx_queue_len` remains non-zero (`2`)
  - `LSR=0x61` and `IIR=0x04` indicate queued RX + pending RDA
  - but firmware path shown in trace does not consume those bytes in the blocked loop.

## Expected board semantics (trace-backed)

- Host issues telephony control/query commands and expects telephony status-class responses.
- Boot-critical checks rely on receiving telephony status information (not only UART configured state).
- Sanity modem-line states (`CTS/DSR/DCD`) and interrupt/read semantics must remain coherent, but they are not sufficient alone.

## Current missing piece

- The emulator currently seeds RX bytes, but the byte sequence/phase coupling is still not sufficient to drive firmware through telephony-up transition.
- Exact next fix is command-phase response framing for boot init sequence, ensuring host-side consumption and state transition can happen in-order.
