// SPDX-License-Identifier: GPL-2.0-or-later
// Validates phrase-catalog stub JSON exists (no phrase labels asserted).

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace fs = std::filesystem;

int main()
{
	std::string const root = COINLINE_EMU_SOURCE_DIR;
	fs::path const p = fs::path(root) / "fixtures" / "voiceware" / "phrase-catalog.json";
	if (!fs::exists(p)) {
		std::cerr << "missing " << p.string() << '\n';
		return 1;
	}
	std::ifstream f(p);
	std::string const contents((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	if (contents.find("phrase_catalog_stub") == std::string::npos) {
		std::cerr << "unexpected phrase-catalog marker\n";
		return 1;
	}
	return 0;
}
