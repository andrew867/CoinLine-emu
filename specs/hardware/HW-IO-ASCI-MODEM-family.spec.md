# Umbrella spec: Unknown I/O policy, ASCI, modem, host bridge

Hardware IDs: **HW-IO-001**, **HW-ASCI-001**, **HW-MOD-001**, **HW-HB-001**

## HW-IO-001

- **Policy:** `docs/io-port-map.md` — open-bus defaults, structured unknown-port log, no port-derived magic.  
- **Tests:** `test_unknown_port_logging.cpp`, `test_io_port_defaults.cpp`.  
- **Acceptance:** Every unmapped access logged with PC/cycle when tracing enabled.

## HW-ASCI-001 / HW-MOD-001

- **Sources:** `millennium_modem*.cpp`, `millennium_modem_model.*`.  
- **Line state:** DCD/CTS/RTS/DTR behaviors tied to model + `notify_modem_dcd` from screen/update when carrier simulated.  
- **Traces:** `asci-trace.jsonl`, `uart-transcript.log`, modem lines in `io-trace.jsonl`.  
- **Tests:** `test_uart_transcript.cpp`, `test_modem_state_machine.cpp`, integration modem/host tests.  
- **Unknowns:** Real PSTN impairments — future fixtures; no fictional carrier without model edge.

## HW-HB-001

- **Sources:** `millennium_hostbridge_tcp.cpp`.  
- **Purpose:** TCP framing to CoinLine Host — **not** a high-level terminal mock inside driver.  
- **Tests:** `test_host_bridge_*.cpp`.  
- **Acceptance:** Transcript bytes match wire protocol expectations for harness scenarios.
