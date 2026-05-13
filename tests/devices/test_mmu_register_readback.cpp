// SPDX-License-Identifier: GPL-2.0-or-later
// Confirms the generated register map documents MMU ports 0x38-0x3A (metadata test).

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

std::filesystem::path root() { return std::filesystem::path(COINLINE_EMU_SOURCE_DIR); }

} // namespace

int main()
{
	std::filesystem::path const j = root() / "build/generated/z180-internal-register-map.json";
	std::ifstream in(j);
	if (!in) {
		std::cerr << "missing " << j.string() << "\n";
		return 1;
	}
	std::string const txt((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	for (char const *needle : { "z180_mmu_cbr", "z180_mmu_bbr", "z180_mmu_cbar", "0x38", "0x39", "0x3A" }) {
		if (txt.find(needle) == std::string::npos) {
			std::cerr << "map missing: " << needle << "\n";
			return 1;
		}
	}
	return 0;
}
