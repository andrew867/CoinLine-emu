# Test plan: Keypad, hookswitch, security, MACH PIO

## Unit tests

`test_keypad_matrix`, `test_keypad_scan`, `test_keypad_quick_access`, `test_keypad_volume_language`, `test_hookswitch`, `test_hookswitch_debounce`, `test_security_inputs`, `test_security_service_debounce`, `test_mach_pio_c0`.

## Integration

`test_keypad_smoke`.

## Artifacts

Keypad-related lines in `io-trace.jsonl`; `mach_pio_port_h` tag when firmware hits STATUS paths.
