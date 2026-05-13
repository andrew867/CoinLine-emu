// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_io_shared.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

std::filesystem::path emu_root() { return std::filesystem::path(COINLINE_EMU_SOURCE_DIR); }

} // namespace

int main()
{
	std::ifstream in(emu_root() / "fixtures/board/io-port-map.json");
	if (!in) {
		std::cerr << "cannot open io-port-map.json\n";
		return 1;
	}
	std::string const txt((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	std::array<bool, 256> known{};
	std::uint8_t unk = 0;
	std::string err;
	if (!millennium_io_parse_port_map(txt, unk, known, err)) {
		std::cerr << err << "\n";
		return 1;
	}
	if (unk != 0xff) {
		std::cerr << "unexpected unknown_default\n";
		return 1;
	}
	if (!known[0x40]) {
		std::cerr << "expected port 0x40 to be marked known/suspected in fixture\n";
		return 1;
	}
	return 0;
}
