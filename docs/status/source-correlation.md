# Address correlation

## Primary regions (trace-backed)

From milestone snapshots and CPU observation during bring-up:

- **BOOTCODE** banked **0x5000–0x7FFF** with physical start **0x5000**; **SRAM** at physical **0xC0002**; **STACK** logical **0xFBAE** → physical **0xC7BAE**.

## Symbol hunt

Cross-check interesting PCs against `**[address-source-correlation.json](../../build/generated/address-source-correlation.json)`** (generated when available) and operator traces. This repository does not publish proprietary identifiers or provenance paths.

## Hot PCs

See `build/generated/address-source-correlation.json` — correlated from **trace tags** and milestone snapshots where lines were not automatically resolved.