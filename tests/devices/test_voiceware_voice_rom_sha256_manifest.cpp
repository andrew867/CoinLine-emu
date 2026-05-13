// SPDX-License-Identifier: GPL-2.0-or-later
// T0.1 — pinned SHA-256 for shipped `voice_a.bin` / `voice_b.bin` (1 MiB each) when files are present.

#include "millennium_sha256.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

std::string read_file(std::string const &path, std::string &err)
{
	std::ifstream f(path, std::ios::binary);
	if (!f) {
		err = "open failed: " + path;
		return {};
	}
	std::ostringstream ss;
	ss << f.rdbuf();
	return ss.str();
}

bool extract_json_string(std::string const &j, char const *key, std::string &out)
{
	std::string const needle = std::string("\"") + key + "\"";
	auto p = j.find(needle);
	if (p == std::string::npos)
		return false;
	auto colon = j.find(':', p);
	if (colon == std::string::npos)
		return false;
	auto q1 = j.find('"', colon);
	if (q1 == std::string::npos)
		return false;
	auto q2 = j.find('"', q1 + 1);
	if (q2 == std::string::npos)
		return false;
	out = j.substr(q1 + 1, q2 - q1 - 1);
	return out.size() == 64U;
}

} // namespace

int main()
{
	std::string const root = COINLINE_EMU_SOURCE_DIR;
	std::string const base = root + "/fixtures/voiceware/voice-rom-sha256.expected.json";
	std::string err;
	std::string const j = read_file(base, err);
	if (j.empty()) {
		std::cerr << err << '\n';
		return 77;
	}
	std::string expect_u16, expect_u26;
	if (!extract_json_string(j, "voice_a_sha256", expect_u16) || !extract_json_string(j, "voice_b_sha256", expect_u26)) {
		std::cerr << "manifest parse failed\n";
		return 1;
	}

	std::string romdir = root + "/roms/cl_millennium";
	if (char const *d = std::getenv("COINLINE_VOICE_ROM_DIR"); d && *d)
		romdir = d;
	while (!romdir.empty() && (romdir.back() == '/' || romdir.back() == '\\'))
		romdir.pop_back();

	std::string const p16 = romdir + "/voice_a.bin";
	std::string const p26 = romdir + "/voice_b.bin";
	std::string e2;
	std::string const b16 = read_file(p16, e2);
	std::string const b26 = read_file(p26, e2);
	if (b16.size() != 1048576U || b26.size() != 1048576U) {
		std::cerr << "skip: voice ROM pair not found at " << romdir << " (set COINLINE_VOICE_ROM_DIR)\n";
		return 77;
	}

	std::string const h16 = millennium_sha256_hex(reinterpret_cast<unsigned char const *>(b16.data()), b16.size());
	std::string const h26 = millennium_sha256_hex(reinterpret_cast<unsigned char const *>(b26.data()), b26.size());
	if (h16 != expect_u16 || h26 != expect_u26) {
		std::cerr << "SHA mismatch\nU16 got " << h16 << "\nU26 got " << h26 << '\n';
		return 1;
	}
	return 0;
}
