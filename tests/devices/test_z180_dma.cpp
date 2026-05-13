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
	std::ifstream in(emu_root() / "fixtures/z180/dma-registers.json");
	if (!in) {
		std::cerr << "missing dma-registers fixture\n";
		return 1;
	}
	std::string const txt((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	for (char const *k : { "\"DSTAT\"", "\"DMODE\"", "\"DCNTL\"" }) {
		if (txt.find(k) == std::string::npos) {
			std::cerr << "missing DMA register key " << k << "\n";
			return 1;
		}
	}
	return 0;
}
