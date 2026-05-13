// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_firmware.h"
#include "millennium_sha256.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::filesystem::path emu_root()
{
	return std::filesystem::path(COINLINE_EMU_SOURCE_DIR);
}

} // namespace

int main()
{
	std::filesystem::path const fw = emu_root().parent_path() / "firmware" / "flash.bin";
	if (!std::filesystem::exists(fw)) {
		std::cerr << "skip: firmware not present at " << fw.string() << "\n";
		return 77;
	}

	std::vector<std::uint8_t> bytes;
	std::string err;
	if (!millennium_read_file(fw.string(), bytes, err)) {
		std::cerr << "read failed: " << err << "\n";
		return 1;
	}

	std::vector<std::uint8_t> board_raw;
	if (!millennium_read_file((emu_root() / "fixtures/board/board-profile-2line-vfd.json").string(), board_raw,
		    err)) {
		std::cerr << "board read failed: " << err << "\n";
		return 1;
	}
	std::string const board_txt(reinterpret_cast<char const *>(board_raw.data()), board_raw.size());

	std::vector<std::uint8_t> hashes_raw;
	if (!millennium_read_file((emu_root() / "fixtures/firmware/firmware-hashes.json").string(), hashes_raw,
		    err)) {
		std::cerr << "hashes read failed: " << err << "\n";
		return 1;
	}
	std::string const hashes_txt(reinterpret_cast<char const *>(hashes_raw.data()), hashes_raw.size());

	auto const vr = millennium_validate_firmware(bytes, board_txt, hashes_txt);
	if (!vr.ok) {
		std::cerr << "validate failed: " << vr.error << "\n";
		return 1;
	}
	char const *const expect = "b09f9c64817f52522cdb4a01f43cdfe5422eb65cd087defeec2906e597d60e34";
	if (vr.sha256_hex != expect) {
		std::cerr << "unexpected sha256: " << vr.sha256_hex << "\n";
		return 1;
	}
	if (millennium_sha256_hex(bytes.data(), bytes.size()) != expect) {
		std::cerr << "sha256 helper mismatch\n";
		return 1;
	}
	return 0;
}
