// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace {

std::filesystem::path root() { return std::filesystem::path(COINLINE_EMU_SOURCE_DIR); }

} // namespace

int main()
{
	std::filesystem::path const lay = root() / "artwork/millennium.lay";
	std::filesystem::path const png = root() / "artwork/millennium-terminal-front.png";
	std::filesystem::path const vfd_font = root() / "artwork/Millenft.ttf";
	std::ifstream layin(lay, std::ios::binary);
	std::ifstream pngin(png, std::ios::binary);
	std::ifstream ttfin(vfd_font, std::ios::binary);
	if (!layin || !pngin || !ttfin) {
		std::cerr << "missing artwork file (lay, png, or Millenft.ttf)\n";
		return 1;
	}
	std::string laytxt((std::istreambuf_iterator<char>(layin)), std::istreambuf_iterator<char>());
	if (laytxt.find("<mamelayout") == std::string::npos || laytxt.find("millennium-terminal-front.png") == std::string::npos) {
		std::cerr << "unexpected .lay contents\n";
		return 1;
	}
	unsigned char sig[8] = {};
	pngin.read(reinterpret_cast<char *>(sig), 8);
	static unsigned char const png_magic[8] = { 0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a };
	if (pngin.gcount() < 8 || !std::equal(png_magic, png_magic + 8, sig)) {
		std::cerr << "PNG magic missing\n";
		return 1;
	}
	return 0;
}
