# Umbrella spec: VFD (2-line and optional 11-line)

Hardware IDs: **HW-VFD-001**, **HW-VFD-002**

## Shared behavior

| Field | Detail |
| ----- | ------ |
| **Purpose** | LCD/VFD controller accessed via **DISPLAY_PORT 0x60** (board-specific command/data protocol). |
| **Firmware-facing** | Writes to `0x60` carry command vs data per internal driver state machine (`millennium_vfd*.cpp`). |
| **Reset state** | Blank or idle pattern per `fixtures/display/*.json` — **not** hard-coded marketing strings at milestone claim. |
| **Read behavior** | Status path (`vfd_status_r`) returns busy/ready per model. |
| **MAME sources** | `millennium_vfd.cpp`, `millennium_vfd_model.cpp`, `millennium.lay`, `screen_device`. |
| **Trace events** | `vfd-trace.jsonl` when enabled; boot trace milestones **M6** / **M10** require real firmware writes. |
| **Evidence** | PNG screenshot + `boot-trace.jsonl` lines tying PC to `0x60` data cycles. |
| **Tests** | `test_vfd_command_decode.cpp`, `test_vfd_2line_idle.cpp`, `test_vfd_buffer_snapshot.cpp`, `test_vfd_11line_ad.cpp`. |

## HW-VFD-001 — 2-line primary UI

| Field | Detail |
| ----- | ------ |
| **Implementation acceptance** | **M6** milestone emitted only when firmware performs qualifying display write (validator definition). |
| **Current gap** | Reference run **M5** — **M6 not reached** (`boot-milestone-status.json`). |
| **Field validation** | Photo comparison of hardware unit vs MAME `screen_update` under same inputs. |

## HW-VFD-002 — 11-line / advertising profile

| Field | Detail |
| ----- | ------ |
| **Board profile** | `fixtures/board/board-profile-11line-vfd.json`. |
| **Implementation acceptance** | Scenario + screenshot proves firmware selects 11-line mode (not fixture-only). |
| **Unknowns** | Exact controller RAM depth — document from datasheet evidence when available. |
