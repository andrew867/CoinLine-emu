// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {

std::filesystem::path emu_root() { return std::filesystem::path(COINLINE_EMU_SOURCE_DIR); }

} // namespace

int main()
{
	std::array<unsigned char, 5> const kPrefix = { 0xf3, 0x4d, 0xc3, 0x71, 0x01 };
	if (kPrefix[0] != 0xf3 || kPrefix[2] != 0xc3)
		return 1;

	std::filesystem::path const fw = emu_root().parent_path() / "firmware" / "flash.bin";
	if (std::filesystem::exists(fw)) {
		std::ifstream in(fw, std::ios::binary);
		std::array<unsigned char, 5> buf{};
		if (!in.read(reinterpret_cast<char *>(buf.data()), buf.size())) {
			std::cerr << "failed to read prefix\n";
			return 1;
		}
		if (buf != kPrefix) {
			std::cerr << "instruction prefix mismatch\n";
			return 1;
		}
	}

	return 0;
}
