// SPDX-License-Identifier: GPL-2.0-or-later
// Class A: validates fixtures/board/voiceware-command-map.json shape for emulator wiring.

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

std::filesystem::path root() { return std::filesystem::path(COINLINE_EMU_SOURCE_DIR); }

bool contains(std::string const &hay, char const *needle)
{
	return hay.find(needle) != std::string::npos;
}

} // namespace

int main()
{
	std::filesystem::path const p = root() / "fixtures/board/voiceware-command-map.json";
	std::ifstream in(p);
	if (!in) {
		std::cerr << "missing fixture " << p.string() << '\n';
		return 1;
	}
	std::string const txt((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	if (!contains(txt, "\"0x40\"") || !contains(txt, "\"0x42\"") || !contains(txt, "\"0x61\"")) {
		std::cerr << "voiceware-command-map missing expected ports\n";
		return 1;
	}
	if (!contains(txt, "reset_pulse_min_usec") || !contains(txt, "inter_segment_tick_ms")) {
		std::cerr << "voiceware-command-map missing timing_notes keys\n";
		return 1;
	}
	return 0;
}
