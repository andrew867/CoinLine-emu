// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_z180_mmu.h"

void millennium_z180_mmu_build_table(std::uint8_t cbr, std::uint8_t bbr, std::uint8_t cbar, std::uint32_t mmu[16])
{
	// Ported from z180ops.h z180_device::z180_mmu() (MAME).
	std::uint32_t page = 0;
	std::uint32_t const bb = cbar & 15U;
	std::uint32_t const cb = cbar >> 4U;
	for (page = 0; page < 16; page++) {
		std::uint32_t addr = page << 12;
		if (page >= bb) {
			if (page >= cb)
				addr += (static_cast<std::uint32_t>(cbr) << 12);
			else
				addr += (static_cast<std::uint32_t>(bbr) << 12);
		}
		mmu[page] = static_cast<std::uint32_t>(addr & 0xfffffU);
	}
}

std::uint32_t millennium_z180_mmu_translate20(std::uint16_t logical16, std::uint8_t cbr, std::uint8_t bbr, std::uint8_t cbar)
{
	std::uint32_t mmu[16];
	millennium_z180_mmu_build_table(cbr, bbr, cbar, mmu);
	std::uint32_t const a = static_cast<std::uint32_t>(logical16);
	return mmu[(a >> 12) & 15U] | (a & 0xfffU);
}
