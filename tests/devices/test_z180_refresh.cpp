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
	std::ifstream in(emu_root() / "fixtures/z180/refresh-rcr.json");
	if (!in) {
		std::cerr << "missing refresh-rcr fixture\n";
		return 1;
	}
	std::string const txt((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	if (txt.find("\"rcr_register_relative\"") == std::string::npos) {
		std::cerr << "missing RCR entry\n";
		return 1;
	}
	return 0;
}
