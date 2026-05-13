// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_board_profile.h"
#include "millennium_nvram_model.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

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

	std::vector<std::uint8_t> flash(0x100000, 0xff);
	for (std::size_t i = 0; i < 256 && i < layout.dla_stage_size; ++i) {
		if (!model.write_dla(static_cast<std::uint32_t>(i), static_cast<std::uint8_t>(i), err)) {
			std::cerr << "staging fill: " << err << '\n';
			return 1;
		}
	}

	if (!model.apply_dla_to_flash(flash, err)) {
		std::cerr << "apply: " << err << '\n';
		return 1;
	}
	for (int i = 0; i < 256; ++i) {
		if (flash[std::size_t(i)] != static_cast<std::uint8_t>(i)) {
			std::cerr << "flash prefix mismatch at " << i << '\n';
			return 1;
		}
	}

	// Optional operator firmware blob (not committed): exercise larger apply when provided.
	if (char const *fw = std::getenv("COINLINE_TEST_FIRMWARE")) {
		std::vector<std::uint8_t> fw_bytes;
		std::string ferr;
		std::ifstream inf(fw, std::ios::binary);
		if (!inf) {
			std::cerr << "COINLINE_TEST_FIRMWARE set but file not readable\n";
			return 1;
		}
		inf.seekg(0, std::ios::end);
		auto const sz = inf.tellg();
		if (sz <= 0) {
			std::cerr << "empty COINLINE_TEST_FIRMWARE\n";
			return 1;
		}
		inf.seekg(0, std::ios::beg);
		fw_bytes.resize(std::size_t(sz));
		if (!inf.read(reinterpret_cast<char *>(fw_bytes.data()), std::streamsize(fw_bytes.size()))) {
			std::cerr << "read COINLINE_TEST_FIRMWARE failed\n";
			return 1;
		}
		std::size_t const n = std::min(fw_bytes.size(), static_cast<std::size_t>(layout.dla_stage_size));
		for (std::size_t i = 0; i < n; ++i) {
			if (!model.write_dla(static_cast<std::uint32_t>(i), fw_bytes[i], err))
				return 1;
		}
		std::vector<std::uint8_t> flash2(0x100000, 0xff);
		if (!model.apply_dla_to_flash(flash2, err))
			return 1;
		for (std::size_t i = 0; i < n; ++i) {
			if (flash2[i] != fw_bytes[i])
				return 1;
		}
	}

	return 0;
}
