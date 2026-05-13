// SPDX-License-Identifier: GPL-2.0-or-later
// Verifies read-mask helpers used by io-trace / z180 internal decode (no MAME CPU link).

#include "millennium_z180_register_math.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

std::filesystem::path root() { return std::filesystem::path(COINLINE_EMU_SOURCE_DIR); }

} // namespace

int main()
{
	// ITC: raw | ~0xC7
	if (millennium_z180_itc_read_byte(0x00) != std::uint8_t(0x00 | ~0xC7)) {
		std::cerr << "ITC read merge mismatch\n";
		return 1;
	}
	// BBR is unmasked in trace helper; register_math is for ITC/RCR/IL/IOCR/OMCR
	if (millennium_z180_omcr_read_byte(0x00) != 0x5F) {
		std::cerr << "OMCR read merge mismatch\n";
		return 1;
	}
	std::filesystem::path const j = root() / "build/generated/z180-internal-register-map.json";
	std::ifstream in(j);
	if (!in) {
		std::cerr << "missing " << j.string() << "\n";
		return 1;
	}
	std::string const txt((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	if (txt.find("\"trace_tag\": \"z180_mmu_bbr\"") == std::string::npos) {
		std::cerr << "JSON map missing z180_mmu_bbr\n";
		return 1;
	}
	if (txt.find("z180_internal_register_map") == std::string::npos && txt.find("z180.internal_register") == std::string::npos) {
		std::cerr << "JSON map schema unexpected\n";
		return 1;
	}
	return 0;
}
