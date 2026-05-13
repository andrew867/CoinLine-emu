// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_z180_mmu.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace {

std::filesystem::path emu_root() { return std::filesystem::path(COINLINE_EMU_SOURCE_DIR); }

} // namespace

int main()
{
	// Reset vector first byte (matches test_reset_vector / board bring-up).
	std::filesystem::path const fw = emu_root().parent_path() / "firmware" / "flash.bin";
	if (!std::filesystem::exists(fw))
		return 77;
	std::ifstream in(fw, std::ios::binary);
	unsigned char b = 0;
	if (!in.read(reinterpret_cast<char *>(&b), 1))
		return 1;
	if (b != 0xf3U) {
		std::cerr << "reset vector not DI\n";
		return 1;
	}

	// MMU at reset (from boot-milestones M4 snapshot style): CBR=BBR=0, CBAR=0xF0 — logical 0 maps to phys 0.
	std::uint32_t const t0 = millennium_z180_mmu_translate20(0x0000, 0x00, 0x00, 0xf0);
	if (t0 != 0x00000U) {
		std::cerr << "reset MMU identity low failed\n";
		return 1;
	}

	return 0;
}
