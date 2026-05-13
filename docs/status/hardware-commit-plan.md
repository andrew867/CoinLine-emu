# Hardware work — commit plan

## Git hygiene

1. **`master` carries unrelated local edits** — Before large hardware passes, use **`wip: preserve emulator state before hardware completion pass`** only when you must snapshot dirty trees (see mission Phase 4).  
2. **One logical commit per major tranche** — Matches mission prefixes (`emu-hw:`, `emu-z180:`, `emu-vfd:`, …).  
3. **Commit body** must list: hardware IDs touched, tests run (`ctest`/PowerShell), run folder path if firmware proof involved, remaining blockers.  
4. **Do not** commit generated `build/runs/*`, WAV, PNG, unless explicitly adopting them as **tracked fixtures** (normally **never**).  

## Order (recommended)

| Step | Action |
| ---- | ------ |
| 1 | Commit **audit-only** (`docs/status/hardware-*`, `specs/hardware/*`, `test-plans/hardware/*`) → `emu-hw: audit hardware completion state` |
| 2 | H1 memory/MM fixes → `emu-z180: …` |
| 3 | H2 IRQ/timer → `emu-z180: interrupt and timer tracing` |
| 4 | H3 modem/UART → `emu-modem: …` |
| 5 | H4 VFD/M6 → `emu-vfd: …` |
| 6 | Continue per [`hardware-tranche-plan.md`](hardware-tranche-plan.md) |

## Updating `commit_after_completion`

After each tranche commit, set `commit_after_completion` in [`hardware-gap-register.json`](hardware-gap-register.json) for closed IDs (git full hash).

## Build-or-docs rule

- Code commits **must build** (`build-mame-coinline.ps1`).  
- Docs-only commits: message footer `Docs-only: no build impact.` when true.
