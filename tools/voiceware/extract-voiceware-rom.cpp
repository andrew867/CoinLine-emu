// SPDX-License-Identifier: GPL-2.0-or-later
// Minimal ROM byte reporting for Voiceware images (honest: no full muCCC decode in-tree yet).

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

static bool read_file(char const *path, std::vector<std::uint8_t> &out)
{
	std::ifstream f(path, std::ios::binary);
	if (!f)
		return false;
	f.seekg(0, std::ios::end);
	auto const sz = static_cast<std::size_t>(f.tellg());
	f.seekg(0);
	out.resize(sz);
	if (sz)
		f.read(reinterpret_cast<char *>(out.data()), static_cast<std::streamsize>(sz));
	return static_cast<bool>(f);
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		std::cerr << "usage: extract-voiceware-rom <U16_or_U26.bin>\n";
		return 2;
	}
	std::vector<std::uint8_t> rom;
	if (!read_file(argv[1], rom)) {
		std::perror(argv[1]);
		return 1;
	}
	std::cout << "{\"schema\":\"coinline.voiceware_extraction_stub/v1\",\"path\":\"" << argv[1] << "\",\"size\":"
		  << rom.size() << ",\"first64_hex\":\"";
	char hex[3];
	for (unsigned i = 0; i < 64U && i < rom.size(); ++i) {
		std::snprintf(hex, sizeof(hex), "%02x", unsigned(rom[i]));
		std::cout << hex;
	}
	std::cout << "\",\"decode\":\"not_implemented_full_muCCC_pipeline\""
			  << ",\"note\":\"Use ROM-backed emulator traces + phrase-catalog correlation for semantics.\"}\n";
	return 0;
}
