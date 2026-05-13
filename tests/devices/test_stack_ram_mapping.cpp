// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_z180_mmu.h"

#include <iostream>

int main()
{
	// Stack SP=0x6D57 (trace): with BBR=0 and CBAR=0x85, page 6 uses bank base — physical 0x06D57.
	std::uint32_t const p = millennium_z180_mmu_translate20(0x6D57, 0xB8, 0x00, 0x85);
	if (p != 0x06D57U) {
		std::cerr << "expected phys 0x06D57 for logical 0x6D57 BBR=0\n";
		return 1;
	}
	// DLAPP.MAP stack logical ~0xFBAE → physical 0xC7BAE when mapped through common area.
	std::uint32_t const st = millennium_z180_mmu_translate20(0xFBAE, 0xB8, 0x4D, 0x85);
	if (st != 0xC7BAEU) {
		std::cerr << "expected phys 0xC7BAE for logical stack got 0x" << std::hex << st << '\n';
		return 1;
	}
	return 0;
}
