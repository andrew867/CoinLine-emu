# Test plan: I/O logging, ASCI, modem, host bridge

## Unit / fixture tests

`test_unknown_port_logging`, `test_io_port_defaults`, `test_uart_transcript`, `test_modem_state_machine`, `test_modem_clean_connect_fixture`, `test_modem_m8_twice`.

## Integration

`test_modem_connect`, `test_host_bridge_loopback`, `test_host_bridge_carrier_loss`, `test_host_bridge_noisy_line`, `test_host_bridge_sideband_negotiate`, `test_host_bridge_no_sideband_default`.

## Artifacts

`asci-trace.jsonl`, `uart-transcript.log`, relevant `io-trace.jsonl` slices, host-bridge transcript when scenario uses TCP.

## Failure criteria

Magic modem bytes tied to scenario step without going through ASCI/model path.
