// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_vfd_model.h"
#include "vfd_fixture.hpp"

#include <filesystem>
#include <iostream>

namespace {

std::filesystem::path root() { return std::filesystem::path(COINLINE_EMU_SOURCE_DIR); }

} // namespace

int main()
{
	std::filesystem::path const path = root() / "fixtures/display/vfd-2line-idle.json";
	std::string const file = coinline_read_all_text(path);
	auto const bytes = vfd_parse_hex_raw_field(file);

	millennium_display_profile prof{};
	prof.rows = 2;
	prof.columns = 20;
	prof.variant = "2line";

	millennium_vfd_model v;
	v.configure(prof);
	v.reset();

	std::uint64_t ccy = 0;
	for (std::uint8_t b : bytes)
		v.write(b, ccy++);

	if (!v.text_rows_match_fixture_json(file) || v.raw_log() != bytes) {
		std::cerr << "snapshot text/raw mismatch fixture\n";
		return 1;
	}
	return 0;
}
