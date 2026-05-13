// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_z180_snapshot.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

std::filesystem::path emu_root() { return std::filesystem::path(COINLINE_EMU_SOURCE_DIR); }

} // namespace

int main()
{
	std::ifstream in(emu_root() / "fixtures/z180/prt-registers.json");
	if (!in) {
		std::cerr << "missing prt-registers fixture\n";
		return 1;
	}
	std::string const txt((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	if (txt.find("\"TCR\"") == std::string::npos || txt.find("\"RLDR0L\"") == std::string::npos) {
		std::cerr << "unexpected PRT fixture contents\n";
		return 1;
	}

	millennium_z180_snapshot s{};
	s.tcr = 0x12;
	s.rldr0 = 0x1234;
	s.tmdr0 = 0x5678;
	std::string const m4 = millennium_format_boot_m4("2026-05-03T00:00:00Z", s);
	if (m4.find("TCR=0x12") == std::string::npos || m4.find("RLDR0=0x1234") == std::string::npos) {
		std::cerr << "M4 formatter missing expected PRT fields\n";
		return 1;
	}
	return 0;
}
