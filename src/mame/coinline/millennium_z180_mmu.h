// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>

/// Matches \c z180_device::z180_mmu / MMU_REMAP_ADDR in MAME \c z180ops.h (HD64180/Z180).
void millennium_z180_mmu_build_table(std::uint8_t cbr, std::uint8_t bbr, std::uint8_t cbar, std::uint32_t mmu[16]);

/// Translate a 16-bit logical CPU address to a 20-bit physical address using MMU registers.
std::uint32_t millennium_z180_mmu_translate20(std::uint16_t logical16, std::uint8_t cbr, std::uint8_t bbr, std::uint8_t cbar);
