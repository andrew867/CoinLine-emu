# Z180 Internal Readback Regression Check

Source run: `build/runs/20260505T073045-boot-critical`

Latest verification run: `build/runs/20260505T085920-boot-critical`

## Answer

Is firmware-visible port `0x0039` readback coherent with BBR?

Answer: yes for the current local run. The provided run does not reproduce the stale `z180_internal_bus`/`0xFF` regression. Port `0x0039` reads are decoded as `z180_mmu_bbr` and return `0x00` before the firmware writes `0x4D`, then `0x4D` while the sampled CPU register state is `BBR=0x4D`. No `0x0039` read returns `0xFF` in this run.

## Current Evidence

| Port | Register | Owner | Firmware readback | Firmware writes | Trace tags | `0xFF` readback |
| --- | --- | --- | --- | --- | --- | --- |
| `0x0034` | ITC | MAME Z180 CPU internal register; board catch-all observes external bus only | `0x38` x3045 | none | `z180_itc` x3045 | no |
| `0x0036` | RCR | MAME Z180 CPU internal register; board catch-all observes external bus only | none observed | `0x00` x3045 | `z180_rcr` x3045 | no reads |
| `0x0038` | CBR | MAME Z180 CPU internal register; board catch-all observes external bus only | none observed | `0xB8` x12179 | `z180_mmu_cbr` x12179 | no reads |
| `0x0039` | BBR | MAME Z180 CPU internal register; board catch-all observes external bus only | `0x00` x6090, `0x4D` x6088 | `0x00` x3045, `0x4D` x3045, `0x03` x3044 | `z180_mmu_bbr` x21312 | no |
| `0x003A` | CBAR | MAME Z180 CPU internal register; board catch-all observes external bus only | none observed | `0x85` x12179 | `z180_mmu_cbar` x12179 | no reads |
| `0x003F` | IOCR | MAME Z180 CPU internal register; board catch-all observes external bus only | none observed | `0x00` x3045 | `z180_iocr` x3045 | no reads |

`z180-register-trace.jsonl` samples agree with the decoded internal state over the run:

| Register | Sampled values |
| --- | --- |
| ITC | raw `0x0` x36003; firmware read formula is `ITC | ~0xC7`, so readback `0x38` is expected |
| RCR | raw `0x0` x36003 |
| CBR | `0xB8` x36003 |
| BBR | `0x4D` x35828, `0x00` x109, `0x03` x66 |
| CBAR | `0x85` x36003 |
| IOCR | `0x0` x36003 |

## Ownership Determination

MAME already implements these Z180 internal registers. `z180_device::IN()` checks `is_internal_io_address(port)`, and internal reads call `z180_readcontrol(port)`. That function performs `m_io.read_byte(port)` as an external bus observation and then returns `z180_internal_port_read(port & 0x3f)` to the firmware. Writes similarly call the external bus write and then `z180_internal_port_write(...)`.

The CoinLine board catch-all is therefore a trace-only observer for Z180 internal ports. It must not be considered the source of firmware-visible data. The current driver classifies known ports with decoded tags and uses `millennium_z180_trace_read_byte()` so the I/O trace mirrors the CPU's internal read formulas instead of logging open-bus `0xFF`.

## Previous Runs

Local boot-critical runs scanned under `build/runs` also show coherent readback for the available recent captures. No local run with `0x0039` reads stuck at `0xFF` and tag `z180_internal_bus` was found. The only remaining false-positive evidence in the latest run is `vfd-trace.jsonl`, which was filtered from `io-trace.jsonl` using a pattern that admitted `z180_mmu_*` traffic; that filter has been fixed so VFD traces no longer contain MMU/internal lines.

## Bring-up Status From This Evidence

The current M6 blocker is not Z180 internal MMU/ITC readback. In the latest 30s run, port `0x0039` reads `0x00/0x4D` with tag `z180_mmu_bbr`, port `0x0034` reads `0x39` with tag `z180_itc`, and no read of either port returns `0xFF`.

The current run remains at M5A with no firmware write to port `0x0060`, no M6, no M10, no sampled EI opcode, and IFF1 never sampled true. The active blocker is the repeated `PC 0xFFFF` / `RST38` / `_int0_handler` path that replays voice phrase `0xB3` before the firmware reaches timer/VFD initialization.
