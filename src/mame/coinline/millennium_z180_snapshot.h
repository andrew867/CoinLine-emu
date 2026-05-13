// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <string>

// Host-side Z180 register bundle for M4 JSONL (values are post-reset / post-run snapshots).
struct millennium_z180_snapshot {
	std::uint8_t cbr = 0, bbr = 0, cbar = 0, rcr = 0;
	std::uint8_t cntla0 = 0, cntlb0 = 0, stat0 = 0;
	std::uint8_t tcr = 0;
	std::uint16_t rldr0 = 0, tmdr0 = 0;
	std::uint8_t il = 0, itc = 0;
	std::uint8_t dstat = 0, dmode = 0, dcntl = 0;
	std::uint8_t iocr = 0;
};

std::string millennium_format_boot_m4(std::string const &ts, millennium_z180_snapshot const &s);

std::string millennium_hex_byte(std::uint8_t v);
std::string millennium_hex_word(std::uint16_t v);
