// SPDX-License-Identifier: GPL-2.0-or-later

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

std::filesystem::path emu_root() { return std::filesystem::path(COINLINE_EMU_SOURCE_DIR); }

} // namespace

int main()
{
	std::ifstream in(emu_root() / "fixtures/z180/icr-placement.json");
	if (!in) {
		std::cerr << "missing icr-placement fixture\n";
		return 1;
	}
	std::string const txt((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	if (txt.find("\"default_icr_value_hex\": \"0x40\"") == std::string::npos) {
		std::cerr << "unexpected ICR default in fixture\n";
		return 1;
	}
	if (txt.find("\"base_inclusive_hex\": \"0x40\"") == std::string::npos) {
		std::cerr << "unexpected internal window base\n";
		return 1;
	}
	return 0;
}
