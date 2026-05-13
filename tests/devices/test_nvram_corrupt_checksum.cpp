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

	std::string nv_txt;
	if (!load_text(emu_root() / "fixtures/nvram/corrupt-checksum.nvram.json", nv_txt)) {
		std::cerr << "cannot read corrupt nvram fixture\n";
		return 1;
	}

	millennium_nvram_model model{};
	model.configure(layout);
	if (!model.load_envelope_json(nv_txt, err)) {
		std::cerr << "expected load with mismatch note: " << err << '\n';
		return 1;
	}
	if (!model.checksum_failure()) {
		std::cerr << "expected checksum_failure after corrupt fixture\n";
		return 1;
	}
	if (model.verify_checksum(err)) {
		std::cerr << "verify_checksum should fail before recovery\n";
		return 1;
	}

	model.recover_default_cleared(err);
	if (model.checksum_failure() || !model.verify_checksum(err)) {
		std::cerr << "recovery did not restore valid checksum state\n";
		return 1;
	}

	return 0;
}
