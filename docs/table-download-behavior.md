# Table download behavior

This document describes the firmware-driven flow for receiving downloaded tables from CoinLine Host and persisting them to the table-storage region.

## Purpose

- Receive table contents over the modem UART path.
- Write them to the dedicated table storage sub-region.
- Verify and acknowledge receipt to CoinLine Host.

## High-level flow

```
firmware -> modem TX -> host bridge -> CoinLine Host
firmware <- modem RX <- host bridge <- CoinLine Host
firmware -> table_storage write
firmware -> ack frame -> host bridge -> CoinLine Host
```

## Storage layout

The table storage region (`tablestore` per [`memory-map.md`](memory-map.md)) holds one or more tables, each with a header and payload per the firmware's table convention. The exact internal layout is in the firmware evidence inventory.

## Persistence

Table storage is persisted along with NVRAM ([`nvram-and-table-storage.md`](nvram-and-table-storage.md)). After a reset, downloaded tables are still present.

## Failure modes

| Failure | Expected firmware behavior |
| ------- | -------------------------- |
| Lost carrier mid-download | Firmware aborts; existing tables intact. |
| Bad checksum on a table | Firmware rejects the table; logs a DLOG entry. |
| Table storage full | Firmware rejects further tables. |

## Tests

- `tests/integration/test_table_download_e2e.cpp` — full table-distribution scenario against running CoinLine Host.
- `tests/devices/test_table_storage_region.cpp` — bounded writes only.
- `tests/integration/test_table_download_carrier_loss.cpp` — carrier-loss mid-transfer.

## Scenarios

- `fixtures/scenarios/table-download.json` drives a full download flow and asserts:
  - M11 reached (host call attempted).
  - M12 reached (table storage accessed).
  - Post-reset NVRAM contains the new table.
  - DLOG entry reflects the downloaded table id.

## Acceptance criteria

- Tables persist across reset.
- Failure modes produce the firmware's expected behavior.
- Round-trip with the host application's table-distribution flow succeeds.

## Cross-references

- [`nvram-and-table-storage.md`](nvram-and-table-storage.md).
- [`modem-uart-host-bridge.md`](modem-uart-host-bridge.md).
- [`host-integration-plan.md`](host-integration-plan.md).
- [`../test-plans/table-download-tests.md`](../test-plans/table-download-tests.md).
