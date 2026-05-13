# Engine selection

This document compares candidate engines for `coinline-emu` and records the binding recommendation. The chosen engine is the substrate on which the Millennium-compatible Z180 payphone terminal is emulated.

## Decision summary

**Recommendation: MAME-based.** The CoinLine Terminal Emulator is implemented as a MAME machine driver under [`src/mame/coinline/`](file-plan.md), built against an external MAME tree, and shipped as a separate GPL-isolated project per [`../LICENSE-STRATEGY.md`](../LICENSE-STRATEGY.md).

A **clean-room MIT-clean C++** implementation is documented as a fallback track. All specifications, test plans, fixtures, board profiles, scenarios, and evidence bundle schemas are designed to be **engine-agnostic** so that a pivot does not invalidate the documentation set.

## Comparison

| Option | License | Z180 completeness | Device framework | Speed to boot firmware | Risk | Recommendation |
| ------ | ------- | ----------------- | ---------------- | ---------------------- | ---- | -------------- |
| **MAME-based** | GPL-2.0-or-later (BSD-3-Clause for some components) | Production-grade Z180 device with MMU, ASCI, PRT, DMA, INT controller | Mature MAME device tree, artwork (`.lay`), debugger, save state, scripting | Fast — register driver, point to ROM, iterate | License contamination if mis-handled; large dependency tree | **Recommended** |
| **Clean-room C++ (MIT-clean fallback)** | MIT/Apache-2.0 | None until written; permissive Z80 cores need Z180-peripheral glue | Custom — must be authored | Medium — ramp-up cost for MMU, ASCI, PRT, DMA | Cost; longer to first boot | Documented fallback |
| **Clean-room C# / .NET** | MIT/Apache-2.0 | None until written; possible to port a permissive Z80 core | Custom | Slow — managed-runtime overhead, fewer existing references | Cost; ecosystem mismatch with `coinline-emu` field tooling | Not recommended |
| **Permissive Z80 core + custom Z180 peripherals** | MIT/BSD (per chosen Z80 core) | Z80 core only — Z180 internal peripherals must be authored | Custom | Medium | Same as clean-room C++ but with a third-party core seam | Suitable if MIT-clean track is chosen; documented inside the fallback track |
| **Existing GPL Z180 emulators (non-MAME)** | GPL | Variable — typically narrower than MAME | Sparse | Variable | License posture identical to MAME, with less mature device framework | Not recommended |

## Why MAME

- **Z180 coverage.** MAME's `cpu/z180` device implements the full Z180 (Hitachi HD64180-style) instruction set, MMU, ASCI, PRT timers, INT controller, and DMA — none of which is trivial to write from scratch.
- **Device framework.** MAME's device base classes provide reset, I/O read/write, tick (`device_timer_interface`), interrupt generation, state snapshot (save state), and scripting — exactly the contract specified in [`device-model.md`](device-model.md).
- **Artwork system.** The `.lay` file format supports clickable regions and overlays for the front-panel image at `docs/images/terminal-unbranded.webp`, matching the requirements in [`artwork-and-layout.md`](artwork-and-layout.md).
- **Debugger.** MAME's debugger gives free access to memory, I/O ports, registers, traces, watchpoints, and scripting — accelerating bring-up dramatically.
- **Field tooling.** Save state, replay, and the existing scripting hooks support the scenario runner ([`scenario-runner.md`](scenario-runner.md)) and evidence bundle exporter ([`evidence-bundles.md`](evidence-bundles.md)) without re-implementation.
- **Build maturity.** MAME's build system is well-tested across Linux, macOS, and Windows.

## Why a fallback track exists

The MAME track inherits MAME's GPL distribution obligations. If the project ever needs to ship a binary that is bundled inside an MIT-licensed product, or if downstream packaging requires permissive licensing end-to-end, the project pivots to the MIT-clean track. The pivot is feasible because:

- All hardware contracts are described in [`specs/`](../specs/) in engine-agnostic terms.
- All test plans, fixtures, and scenarios are JSON or Markdown and have no engine dependencies.
- Boot milestones and the unknown-port policy are engine-agnostic.

## Pivot conditions

A pivot to the MIT-clean track is triggered if **any** of the following hold:

- The product strategy requires that the emulator ship as part of the MIT `coinline/` distribution.
- A downstream consumer requires permissive licensing end-to-end.
- A blocking bug in MAME's Z180 core cannot be addressed in a reasonable timeframe.
- A regulatory or contractual constraint forbids GPL components.

If pivoted:

- A permissive Z80 core is selected. Candidates include any BSD/MIT/Apache-2.0 Z80 core whose authorship and provenance are auditable.
- The Z180 internal peripherals are authored in `src/cleanroom/` from the public Z180 datasheet.
- Devices are re-implemented under MIT/Apache-2.0.
- The MAME `.lay` artwork is reauthored as the project's own overlay format.
- All [`specs/`](../specs/), [`test-plans/`](../test-plans/), and fixtures continue to apply unchanged.

## Notes on existing emulators

Collateral trees under this workspace (host tooling, packaged firmware drops, legacy packaging dirs, etc.) are **not** emulators by themselves. Only **`coinline-emu`** — loading a licensed ROM into MAME-modeled hardware — satisfies the “boot real firmware on emulated silicon” bar for engine selection.

## Decision record

- Decision date: this document's last update.
- Decision: MAME-based (default).
- **Distribution license for this tree:** the default MAME track applies **GNU General Public License, version 2 or later** to emulator source in this project. The full text is in the repository root [`LICENSE`](../LICENSE); that file points here for the engine decision.
- Reviewers: project maintainers responsible for both `coinline/` and ``.
- Re-evaluation trigger: any of the pivot conditions above.

When changing this decision, update both this file, [`../LICENSE`](../LICENSE), and [`../LICENSE-STRATEGY.md`](../LICENSE-STRATEGY.md) in the same PR.
