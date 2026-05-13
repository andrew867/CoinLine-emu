// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_board_profile.h"
#include "millennium_firmware.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::filesystem::path emu_root() { return std::filesystem::path(COINLINE_EMU_SOURCE_DIR); }

} // namespace

int main()
{
	std::vector<std::uint8_t> raw;
	std::string err;
	if (!millennium_read_file((emu_root() / "fixtures/board/board-profile-2line-vfd.json").string(), raw, err)) {
		std::cerr << err << "\n";
		return 1;
	}
	std::string const board(reinterpret_cast<char const *>(raw.data()), raw.size());
	millennium_z180_board_config cfg{};
	if (!millennium_board_parse_z180_profile(board, cfg, err)) {
		std::cerr << "parse failed: " << err << "\n";
		return 1;
	}
	if (cfg.clock_hz != 12288000U) {
		std::cerr << "unexpected clock_hz\n";
		return 1;
	}
	if (cfg.wait_io != 1) {
		std::cerr << "expected io wait state 1 from board fixture\n";
		return 1;
	}
	return 0;
}
