# PC FFFF Memory Bank Debug

## Fault Translation

For the first `PC=0xFFFF` run:

- Fault PC logical: `0xFFFF`
- Fault PC physical: `0xC7FFF`
- Fault source: `sram_128k`
- Fault fetch byte: `0xFF`
- Stack logical: `0x4FC4`
- Stack physical: `0xC4FC4`
- Stack source: `sram_128k`
- Return popped: `0xFFFF`

## Hardware Model Error

The Z180 MMU translation itself was coherent. The error was below translation: the SRAM physical window callback supplied relative offsets to helpers that require absolute physical addresses. The firmware wrote runtime stack/BSS state, but the emulator discarded it.

## Post-Fix Evidence

`build/runs/20260505T094137-boot-critical` and `build/runs/20260505T094715-boot-critical` do not show the `PC=0xFFFF` first-fault artifact. Stack-control tracing shows normal return addresses instead of `0xFFFF`.
