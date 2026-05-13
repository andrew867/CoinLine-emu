// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct millennium_firmware_validate_result {
	bool ok = false;
	std::string error;
	std::string sha256_hex;
	std::size_t rom_size_expected = 0;
	bool strict_hash = false;
};

// Read entire file (binary). Sets error if unreadable.
bool millennium_read_file(std::string const &path, std::vector<std::uint8_t> &out, std::string &error);

bool millennium_board_parse_rom_size(std::string const &board_json, std::size_t &rom_size_out,
	std::string &error);

bool millennium_board_parse_strict_hash(std::string const &board_json, bool &strict_out,
	std::string &error);

// If strict: sha256 must appear in firmware-hashes.json entries[].sha256_hex (substring match scan).
millennium_firmware_validate_result millennium_validate_firmware(std::vector<std::uint8_t> const &bytes,
	std::string const &board_json_text, std::string const &firmware_hashes_json_text);
