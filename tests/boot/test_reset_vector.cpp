// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {

std::filesystem::path emu_root() { return std::filesystem::path(COINLINE_EMU_SOURCE_DIR); }

} // namespace

int main()
{
	unsigned char const kCanonicalFirst = 0xf3;

	std::filesystem::path const fw = emu_root().parent_path() / "firmware" / "flash.bin";
	if (std::filesystem::exists(fw)) {
		std::ifstream in(fw, std::ios::binary);
		unsigned char b = 0;
		if (!in.read(reinterpret_cast<char *>(&b), 1)) {
			std::cerr << "failed to read firmware first byte\n";
			return 1;
		}
		if (b != kCanonicalFirst) {
			std::cerr << "reset-vector opcode mismatch\n";
			return 1;
		}
	} else if (std::getenv("COINLINE_REQUIRE_FIRMWARE") != nullptr) {
		std::cerr << "missing firmware and COINLINE_REQUIRE_FIRMWARE set\n";
		return 1;
	}

	return 0;
}
