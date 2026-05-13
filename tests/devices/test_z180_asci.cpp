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
	std::ifstream in(emu_root() / "fixtures/z180/asci-registers.json");
	if (!in) {
		std::cerr << "missing asci-registers fixture\n";
		return 1;
	}
	std::string const txt((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	if (txt.find("\"CNTLA0\"") == std::string::npos || txt.find("\"STAT0\"") == std::string::npos) {
		std::cerr << "unexpected ASCI fixture contents\n";
		return 1;
	}
	return 0;
}
