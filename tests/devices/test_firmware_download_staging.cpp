// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_board_profile.h"
#include "millennium_nvram_model.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

std::filesystem::path emu_root() { return std::filesystem::path(COINLINE_EMU_SOURCE_DIR); }

bool load_text(std::filesystem::path const &p, std::string &out)
{
	std::ifstream in(p);
	if (!in)
		return false;
	out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
	return true;
}

} // namespace

int main()
{
	std::string board_txt;
	if (!load_text(emu_root() / "fixtures/board/board-profile-2line-vfd.json", board_txt)) {
		std::cerr << "cannot read board profile\n";
		return 1;
	}
	millennium_memory_layout_config layout{};
	std::string err;
	if (!millennium_board_parse_memory_layout(board_txt, layout, err)) {
		std::cerr << "memory layout: " << err << '\n';
		return 1;
	}

	millennium_nvram_model model{};
	model.configure(layout);

	std::uint32_t const off = 4096;
	std::uint8_t const tag = 0xDA;
	if (!model.write_dla(off, tag, err)) {
		std::cerr << "dla write failed: " << err << '\n';
		return 1;
	}
	if (model.read_dla(off) != tag) {
		std::cerr << "dla readback mismatch\n";
		return 1;
	}

	if (layout.dla_stage_base == 0 || layout.dla_stage_size == 0) {
		std::cerr << "board profile missing dla staging\n";
		return 1;
	}

	return 0;
}
