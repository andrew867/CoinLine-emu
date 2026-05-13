// SPDX-License-Identifier: GPL-2.0-or-later
// Voiceware phrase port and milestone helper shape (no MAME link).

#include "millennium_debug.h"

#include <iostream>
#include <string>

int main()
{
	std::string const line =
		millennium_boot_trace_m5v("2026-05-04T00:00:00Z", "0x5B29", "0xB3");
	if (line.find("\"milestone\":\"M5V\"") == std::string::npos || line.find("0x0061") == std::string::npos) {
		std::cerr << "M5V boot trace shape unexpected\n";
		return 1;
	}
	std::string const vt =
		millennium_format_voiceware_trace_line(12345ULL, 'w', 0xb3U, 0x5b29U, 0x7fd8U, 0x88U, "M5");
	if (vt.find("\"port\":\"0x0061\"") == std::string::npos || vt.find("\"data\":\"0xB3\"") == std::string::npos) {
		std::cerr << "voiceware trace line unexpected\n";
		return 1;
	}
	return 0;
}
