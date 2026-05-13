// SPDX-License-Identifier: GPL-2.0-or-later

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
	std::vector<std::uint8_t> bytes(1048576, 0);
	std::string err;

	std::vector<std::uint8_t> board_raw;
	if (!millennium_read_file((emu_root() / "fixtures/board/board-profile-2line-vfd.json").string(), board_raw,
		    err)) {
		std::cerr << err << "\n";
		return 1;
	}
	std::string const board_txt(reinterpret_cast<char const *>(board_raw.data()), board_raw.size());

	std::vector<std::uint8_t> hashes_raw;
	if (!millennium_read_file((emu_root() / "fixtures/firmware/firmware-hashes.json").string(), hashes_raw,
		    err)) {
		std::cerr << err << "\n";
		return 1;
	}
	std::string const hashes_txt(reinterpret_cast<char const *>(hashes_raw.data()), hashes_raw.size());

	auto const vr = millennium_validate_firmware(bytes, board_txt, hashes_txt);
	if (vr.ok) {
		std::cerr << "expected hash validation failure\n";
		return 1;
	}
	if (vr.error.find("hash") == std::string::npos) {
		std::cerr << "unexpected error: " << vr.error << "\n";
		return 1;
	}
	return 0;
}
