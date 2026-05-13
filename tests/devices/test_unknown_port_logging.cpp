// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_io_shared.h"

#include <iostream>
#include <string>

int main()
{
	std::string const line =
		millennium_format_unknown_port_json("2026-05-03T12:00:00Z", 12345, 0x0102, 0x1245, true, 0xab, nullptr,
			"unknown_port");
	char const *keys[] = { "\"ts\":", "\"cycle\":", "\"pc\":", "\"port16\":", "\"port_lo\":", "\"rw\":", "\"value\":",
		"\"source_symbol\":", "\"note\":" };
	for (auto *k : keys) {
		if (line.find(k) == std::string::npos) {
			std::cerr << "missing key fragment: " << k << "\n";
			return 1;
		}
	}
	if (line.find("\"rw\":\"w\"") == std::string::npos) {
		std::cerr << "expected write direction\n";
		return 1;
	}
	return 0;
}
