// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_z180_register_math.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

std::string read_text(std::filesystem::path const &path)
{
	std::ifstream in(path);
	if (!in)
		return {};
	return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

std::filesystem::path root()
{
	return std::filesystem::path(COINLINE_EMU_SOURCE_DIR);
}

bool contains(std::string const &haystack, char const *needle)
{
	return haystack.find(needle) != std::string::npos;
}

} // namespace

int main()
{
	std::string const report = read_text(root() / "build/generated/z180-internal-readback-regression.json");
	if (report.empty()) {
		std::cerr << "missing z180 internal readback regression report\n";
		return 1;
	}
	if (!contains(report, "\"firmware_visible_port_0x0039_readback_coherent_with_bbr\": true")) {
		std::cerr << "BBR readback report is not coherent\n";
		return 1;
	}
	if (!contains(report, "\"read_returns_0xff\": false")) {
		std::cerr << "readback report does not prove non-0xFF internal reads\n";
		return 1;
	}
	if (contains(report, "\"z180_internal_bus\"")) {
		std::cerr << "generic z180_internal_bus tag leaked into readback report\n";
		return 1;
	}

	std::string const src = read_text(root() / "src/mame/coinline/millennium_state.cpp");
	if (!contains(src, "millennium_z180_trace_read_byte(*m_maincpu, port)")) {
		std::cerr << "catch-all internal read no longer mirrors Z180 state\n";
		return 1;
	}
	if (!contains(src, "millennium_z180_internal_trace_tag")) {
		std::cerr << "catch-all internal trace no longer uses decoded tags\n";
		return 1;
	}

	if (millennium_z180_itc_read_byte(0x00) != std::uint8_t(0x38)) {
		std::cerr << "ITC read mask should return 0x38 for raw 0x00\n";
		return 1;
	}
	return 0;
}
