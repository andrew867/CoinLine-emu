// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "cpu/z180/z180.h"

#include <cstdint>

/// Decode-only helpers: MAME's z180_device already stores internal register state. The I/O space
/// catch-all sees the external bus strobe; these helpers mirror what z180_internal_port_read returns
/// so JSONL traces match CPU-visible data (see z180.cpp cases 0x34–0x3f).

char const *millennium_z180_internal_trace_tag(std::uint8_t port_low6);

/// CoinLine uses \c Z80180 (\c extended_io == false): internal I/O window is 0x40 ports selected by IOCR.
bool millennium_z180_port_is_internal_window(z180_device &cpu, std::uint16_t port_full);

/// Value that appears on the external I/O read bus for this port (often masked). For internal
/// ports this matches z180_internal_port_read formulas where applicable.
std::uint8_t millennium_z180_trace_read_byte(z180_device &cpu, std::uint16_t port_full);

/// Data byte driven during write (same as firmware wrote on the bus).
inline std::uint8_t millennium_z180_trace_write_byte(std::uint8_t data)
{
	return data;
}
