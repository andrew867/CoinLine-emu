// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_z180_mmu.h"

#include <cstdlib>
#include <iostream>

int main()
{
	// MAME z180ops.h — same as firmware trace: CBAR=0x85, CBR=0xB8, BBR=0x4D
	std::uint8_t const cbr = 0xB8;
	std::uint8_t const bbr = 0x4D;
	std::uint8_t const cbar = 0x85;

	std::uint32_t const t6d57 = millennium_z180_mmu_translate20(0x6D57, cbr, bbr, cbar);
	if (t6d57 != 0x53D57U) {
		std::cerr << "translate 0x6D57 expected 0x53D57 got 0x" << std::hex << t6d57 << '\n';
		return 1;
	}
	std::uint32_t const tcfc8 = millennium_z180_mmu_translate20(0xCFC8, cbr, bbr, cbar);
	if (tcfc8 != 0xC4FC8U) {
		std::cerr << "translate 0xCFC8 expected 0xC4FC8 got 0x" << std::hex << tcfc8 << '\n';
		return 1;
	}
	std::uint32_t const t3110 = millennium_z180_mmu_translate20(0x3110, cbr, bbr, cbar);
	if (t3110 != 0x03110U) {
		std::cerr << "translate 0x3110 expected 0x03110 got 0x" << std::hex << t3110 << '\n';
		return 1;
	}

	std::uint32_t mmu[16];
	millennium_z180_mmu_build_table(cbr, bbr, cbar, mmu);
	if (mmu[6] != 0x53000U || mmu[12] != 0xC4000U) {
		std::cerr << "mmu table mismatch\n";
		return 1;
	}

	// BBR=0: bank pages use identity offset for pages 5–7 (DLAPP banked BOOTCODE at phys 0x5000).
	std::uint32_t const t50 = millennium_z180_mmu_translate20(0x5000, cbr, 0x00, cbar);
	if (t50 != 0x05000U) {
		std::cerr << "translate 0x5000 BBR=0 expected 0x05000\n";
		return 1;
	}

	return 0;
}
