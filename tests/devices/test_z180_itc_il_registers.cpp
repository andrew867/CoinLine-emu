// SPDX-License-Identifier: GPL-2.0-or-later
// ITC/IL read masks must stay aligned with MAME z180 internal read behavior (see millennium_z180_register_math.h).

#include "millennium_z180_register_math.h"
#include "millennium_debug.h"
#include "millennium_z180_snapshot.h"

#include <cstdint>
#include <iostream>
#include <string>

int main()
{
	// ITC: upper bits forced on read (per z180.cpp-style mask)
	if (millennium_z180_itc_read_byte(0x00) != std::uint8_t(0x00U | ~0xC7U)) {
		std::cerr << "ITC read merge mismatch\n";
		return 1;
	}
	// IL: only top three bits
	if (millennium_z180_il_read_byte(0xffU) != 0xe0U) {
		std::cerr << "IL read mask mismatch\n";
		return 1;
	}
	millennium_z180_snapshot s{};
	s.itc = 0x39U;
	s.il = 0xabU;
	std::string const line = millennium_format_z180_reg_trace_line(0, 0, 0, false, false, s, "M4");
	if (line.find("\"itc\":\"0x39\"") == std::string::npos || line.find("\"il\":\"0xAB\"") == std::string::npos) {
		std::cerr << "z180 reg trace missing itc/il\n";
		return 1;
	}
	return 0;
}
