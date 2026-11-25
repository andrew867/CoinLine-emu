// SPDX-License-Identifier: GPL-2.0-or-later

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

std::filesystem::path root()
{
	return std::filesystem::path(COINLINE_EMU_SOURCE_DIR);
}

std::string read_text(std::filesystem::path const &path)
{
	std::ifstream in(path);
	if (!in)
		return {};
	return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

bool contains(std::string const &haystack, char const *needle)
{
	return haystack.find(needle) != std::string::npos;
}

} // namespace

int main()
{
	auto const map_path = root() / "build/generated/z180-internal-register-map.json";
	auto const report_path = root() / "build/generated/z180-internal-readback-regression.json";
	if (!std::filesystem::exists(map_path) || !std::filesystem::exists(report_path)) {
		std::cerr << "skip: MAME register evidence is absent\n";
		return 77;
	}
	std::string const map = read_text(map_path);
	std::string const report = read_text(report_path);

	for (char const *needle : { "\"port_low6\": \"0x38\"", "\"port_low6\": \"0x39\"", "\"port_low6\": \"0x3A\"",
			 "z180_mmu_cbr", "z180_mmu_bbr", "z180_mmu_cbar" }) {
		if (!contains(map, needle)) {
			std::cerr << "Z180 MMU register map missing " << needle << "\n";
			return 1;
		}
	}
	for (char const *needle : { "\"0x0038\"", "\"0x0039\"", "\"0x003A\"", "\"0xB8\": 12179",
			 "\"0x4D\": 6088", "\"0x85\": 12179" }) {
		if (!contains(report, needle)) {
			std::cerr << "Z180 MMU readback report missing " << needle << "\n";
			return 1;
		}
	}
	return 0;
}
