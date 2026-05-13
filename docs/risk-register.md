# Risk register

This register tracks risks for `coinline-emu`. Severity, likelihood, and impact are scored qualitatively (Low / Medium / High / Critical). Mitigation is a concrete action with a referenced test. The Tranche column points to the tranche where the mitigation is most effective.

| ID | Risk | Area | Severity | Likelihood | Impact | Mitigation | Test | Tranche |
|----|------|------|----------|------------|--------|------------|------|---------|
| R-03 | Wrong memory map | Bring-up | High | High | Boot loop; silent corruption; confusing failures | Map fixtures `fixtures/board/memory-map.json` validated against schema; cross-check with ``; tests pin region defaults | `tests/devices/test_memory_map.cpp` | E1, E2 |
| R-04 | Wrong I/O default values | Bring-up | High | High | Firmware reads stuck-zero or stuck-one and loops | Unknown-port logging; per-port test rows in `tests/devices/test_io_port_*.cpp`; per-device defaults pinned in `specs/*.spec.md` | `tests/devices/test_io_port_defaults.cpp` | E2, per-device tranches |
| R-05 | Firmware boot loop due to missing peripheral | Bring-up | High | High | M5–M9 not reached; bring-up stalls | Boot-milestone ladder ([`boot-milestones.md`](boot-milestones.md)); per-milestone test; failure mode table maps boot-loop pattern to peripheral | `tests/integration/test_boot_to_idle.cpp` | E2–E10 |
| R-06 | Inaccurate UART/modem behavior | Modem | High | Medium | Host bridge fails or framing diverges from CoinLine Host expectations | ASCI register tests; modem state machine tests; loopback test against recorded fixtures | `tests/devices/test_modem_state_machine.cpp` | E6 |
| R-07 | VFD command mismatch | Display | Medium | Medium | Display garbled or empty; M6 reached only superficially | Command decode tests against `fixtures/display/*.json`; reference 2-line and 11-line snapshots | `tests/devices/test_vfd_command_decode.cpp` | E4 |
| R-08 | Keypad matrix mismatch | Input | Medium | Medium | Firmware does not see keypress events or maps them wrong | Matrix decode test; per-key fixture; quick-access keys covered separately | `tests/devices/test_keypad_matrix.cpp` | E5 |
| R-09 | Card reader timing mismatch | Card | Medium | Medium | Firmware rejects valid swipes or accepts invalid ones | Magstripe timing test using deterministic byte streams; LRC validation test | `tests/devices/test_card_swipe_timing.cpp` | E8 |
| R-10 | Coin pulse timing mismatch | Coin | Medium | Medium | Coins miscounted or denominations mis-detected | Pulse-train test; denomination fixture; coin error fixture | `tests/devices/test_coin_pulse_train.cpp` | E9 |
| R-11 | NVRAM corruption | Storage | High | Low | Firmware refuses to boot or enters service mode unexpectedly after NVRAM rewrite | Persistence test; corrupt-checksum recovery test; checksum behavior pinned in spec | `tests/devices/test_nvram_corrupt_checksum.cpp` | E7 |
| R-12 | Table storage mismatch | Storage | High | Medium | Tables fail to persist or wrong sub-regions overwritten | Region-bounded write test; checksum behavior; round-trip from CoinLine Host | `tests/integration/test_table_download_e2e.cpp` | E7, E12 |
| R-13 | Host bridge framing mismatch | Integration | High | Medium | CoinLine Host rejects or misinterprets bytes from emulator | Loopback test; recorded fixture-driven NCC framing test against CoinLine Host's protocol library running over a TCP loopback | `tests/integration/test_host_bridge_framing.cpp` | E6, E12 |
| R-14 | False validation claims | Process | High | Medium | Compatibility item marked complete without evidence | Every `compatibility-validation-items.md` row references a test or evidence artifact; CI fails if a row's referenced artifact does not exist | `tests/fixtures/test_compatibility_items_have_evidence.py` | E13 |
| R-15 | Emulator diverges from hardware | Validation | High | Medium | CI passes but field deployments fail | Field-validation column distinct from emulator-validation column; hardware validation independently tracked; pinned firmware versions per scenario | per scenario evidence bundle | E12, E13 |

## Risk-handling principles

- **License contamination** is treated as the top risk despite a Medium likelihood: impact is Critical and remediation is expensive after the fact. CI gates fail closed.
- **Bring-up risks** (R-03 through R-05) compound: a wrong memory map or I/O default surfaces as a missing milestone. Boot-milestone tracking is therefore the single best instrument for early detection.
- **Validation risks** (R-14, R-15) require process discipline. Distinguish `code complete`, `emulator validated`, `field validated` per [`compatibility-validation-items.md`](compatibility-validation-items.md). Hardware validation pending **never** means code is missing.

## Risk lifecycle

| State | Meaning |
| ----- | ------- |
| `open` | Identified; mitigation not yet implemented. |
| `mitigated` | Mitigation in place; residual risk acceptable. |
| `accepted` | Risk explicitly accepted; ownership recorded. |
| `closed` | No longer applicable. |

## Reporting

Risks are reviewed at every tranche review. Closure or state changes are recorded in the tranche's exit-criteria notes.
