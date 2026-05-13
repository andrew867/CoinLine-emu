# Umbrella spec: Keypad, hookswitch, security, MACH PIO

Hardware IDs: **HW-KEY-001**, **HW-KEY-002**, **HW-SEC-001**, **HW-MACH-001**

## Keypad / IOPorts

- **Map:** `millennium.cpp` `KEYMATRIX` / `TERMINAL21_SOFTKEYS` feed **TP CSI/UI modeling** (`millennium_state`); `millennium_keypad*.cpp` only models **idle** 8255 A/C reads and IP-comm RTS; PIO 0x41–0x44 in `millennium_io.cpp` (VFD/voice-bank/relay path on **B** writes, not physical keys).  
- **Tests:** `test_keypad_matrix.cpp`, `test_keypad_scan.cpp`, `test_keypad_quick_access.cpp`, `test_keypad_volume_language.cpp`, `test_hookswitch.cpp`, `test_hookswitch_debounce.cpp`, `integration/test_keypad_smoke.cpp`.  
- **Acceptance:** MAME panel bits become TP→CP opcodes / query responses per `terminal_21` family; they do **not** short the CP 8255 matrix sense lines. Audio routing follows CP→TP telephony commands / modem/voice hooks; `**notify_hook_off`** is driven from the debounced telephony hook model (`set_tel_hook_state` / stable bit), not from raw PIO keypad reads.

## Security inputs

- **SECMASK:** Lock, door, vault, service — `millennium_security*.cpp`, `test_security_inputs.cpp`, `test_security_service_debounce.cpp`.

## MACH PIO 0xC0–0xC3

- **Implementation:** `millennium_mach_pio.cpp`; STATUS_PORT / bit semantics per fixtures and traces.  
- **Tests:** `test_mach_pio_c0.cpp`.  
- **Evidence:** `mach_pio_port_h` traces in post-mmu run.  
- **Unknowns:** Firmware revision-specific bit meanings — regression when ROM changes.