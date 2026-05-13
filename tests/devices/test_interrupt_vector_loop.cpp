// SPDX-License-Identifier: GPL-2.0-or-later
// Ensures generated hot-vector / context-switch signature JSON remain present for tooling.

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

std::filesystem::path root() { return std::filesystem::path(COINLINE_EMU_SOURCE_DIR); }

bool contains_all(std::string const &t, std::initializer_list<char const *> needles)
{
	for (char const *n : needles) {
		if (t.find(n) == std::string::npos)
			return false;
	}
	return true;
}

} // namespace

int main()
{
	std::filesystem::path const hv = root() / "build/generated/hot-vector-path.json";
	std::filesystem::path const cs = root() / "build/generated/context-switch-signature.json";
	for (auto const &p : {hv, cs}) {
		std::ifstream in(p);
		if (!in) {
			std::cerr << "missing " << p.string() << "\n";
			return 1;
		}
		std::string const txt((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		if (txt.find("schema") == std::string::npos) {
			std::cerr << "bad json " << p.string() << "\n";
			return 1;
		}
	}
	std::ifstream inh(hv);
	std::string const ht((std::istreambuf_iterator<char>(inh)), std::istreambuf_iterator<char>());
	if (!contains_all(ht, {"0x0038", "0x00CF"})) {
		std::cerr << "hot-vector-path.json missing expected bands\n";
		return 1;
	}
	std::ifstream inc(cs);
	std::string const ct((std::istreambuf_iterator<char>(inc)), std::istreambuf_iterator<char>());
	if (ct.find("xentry_int_sub") == std::string::npos) {
		std::cerr << "context-switch-signature.json missing symbol ref\n";
		return 1;
	}
	return 0;
}
