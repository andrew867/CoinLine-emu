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
	std::ifstream in(emu_root() / "fixtures/z180/int-controller.json");
	if (!in) {
		std::cerr << "missing int-controller fixture\n";
		return 1;
	}
	std::string const txt((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	if (txt.find("\"il_register_relative\"") == std::string::npos) {
		std::cerr << "missing IL register entry\n";
		return 1;
	}
	if (txt.find("im2_vector_order") == std::string::npos) {
		std::cerr << "missing IM2 dispatch placeholder\n";
		return 1;
	}
	return 0;
}
