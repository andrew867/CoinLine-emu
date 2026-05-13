// SPDX-License-Identifier: GPL-2.0-or-later

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

std::filesystem::path emu_root() { return std::filesystem::path(COINLINE_EMU_SOURCE_DIR); }

bool contains_substr(std::string const &hay, char const *needle) { return hay.find(needle) != std::string::npos; }

} // namespace

int main()
{
	std::ifstream in(emu_root() / "fixtures/board/memory-map.json");
	if (!in) {
		std::cerr << "cannot open memory-map.json\n";
		return 1;
	}
	std::string const txt((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	if (!contains_substr(txt, "\"name\": \"ram\"") || !contains_substr(txt, "\"size\": 131072")) {
		std::cerr << "memory-map fixture missing expected RAM region\n";
		return 1;
	}
	return 0;
}
