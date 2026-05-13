// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_z180_register_math.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

std::filesystem::path root()
{
	return std::filesystem::path(COINLINE_EMU_SOURCE_DIR);
}

std::string read_text(std::filesystem::path const &path)
{
	std::ifstream in(path);
	if (!in)
		return {};
	return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

bool contains(std::string const &haystack, char const *needle)
{
	return haystack.find(needle) != std::string::npos;
}

} // namespace

int main()
{
	if (millennium_z180_itc_read_byte(0x00) != std::uint8_t(0x38)) {
		std::cerr << "ITC raw 0x00 should read back as 0x38\n";
		return 1;
	}
	if (millennium_z180_iocr_read_byte(0x00, false) != std::uint8_t(0x1f)) {
		std::cerr << "Z80180 IOCR raw 0x00 should read back as 0x1f\n";
		return 1;
	}

	std::string const report = read_text(root() / "build/generated/z180-internal-readback-regression.json");
	if (report.empty()) {
		// Evidence is produced by a MAME-backed run; skip when running the
		// CMake-only suite without a MAME binary.
		std::cerr << "skipping: Z180 readback regression report not generated\n";
		return 77;
	}
	for (char const *needle : { "\"0x0034\"", "\"z180_itc\"", "\"0x38\": 3045", "\"0x003F\"", "\"z180_iocr\"" }) {
		if (!contains(report, needle)) {
			std::cerr << "ITC/IOCR readback report missing " << needle << "\n";
			return 1;
		}
	}
	return 0;
}
