# Remaining work (updated 2026-05-04)

## Done this pass (2026-05-04 — green MAME + 180 s capture)

| Item | Path / value |
|------|--------------|
| Coinline link | **`SOUNDS["SPEAKER"]` + `SOUNDS["UPD7759"]`** in **`mame-overlays/scripts/target/mame/coinline.lua`**; **`make`** sees **`OS=Windows_NT`** from **`_build_mame_inner.sh`** |
| Build script | `tools/mingw64/build-coinline-mame.sh` |
| EXE | `build/bin/coinline-mame.exe` (`build/build-result.json`) |
| Boot-critical run | `build/runs/20260504T180954-boot-critical` |
| Boot trace | **Empty** milestone JSONL in **`boot-trace.jsonl`** — strict validator fails until milestones emit |
| WAV | **`voiceware-output.wav`** **missing** after `-wavwrite` — investigate audio backend / capture harness |

## Done this pass (`20260504T135115-post-mmu-boot-gate`)

| Item | Path / value |
|------|----------------|
| Firmware SHA-256 | `b09f9c64817f52522cdb4a01f43cdfe5422eb65cd087defeec2906e597d60e34` |
| Reference run | `build/runs/20260504T135115-post-mmu-boot-gate/` (45 s) |
| **mach_pio / 0xC0** | **`mach_pio_port_h`** decode; read path fixes **STATUS_PORT_3** vs raw smartcard stomp (see `mach-pio-c0-debug.md`). |
| **Interrupt samples** | **`interrupt-trace.jsonl`**: **`iff1`/`iff2` false**, **`im`** **0** at 5 ms cadence — **EI not observed** in samples. |
| **M6 / 0x60** | **Still none** — milestone **M5**. |

## P1 — reach M6 (first real `0x60` VFD write)

- **Blocker**: still **no** **`vfd_data`** — **`mach_pio`** fix did not unlock display init in **45 s** window.
- Next: **`asci-trace.jsonl`** vs **`millennium_modem`** / carrier–CTS–STAT0 semantics; **EI + timer IRQ** path if firmware enables interrupts later; **install/DLA/host** conditions from source (`DLASTRT`, telephony task).

## P2 — M7–M10 and screenshots

- Re-run **`run-screenshot-capture.ps1`** at **180 s**, windowed on an interactive desktop if PNG evidence is required.
- Formal validation: `validate-boot-milestones.ps1 -RunDir <dir>` (**no** `-BootTraceSmoke`) → requires **M10**.

## Next command

```bash
# From an MSYS2 MINGW64 shell with the working directory at :
./tools/mingw64/run-boot-critical-capture.sh
```
