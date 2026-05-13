// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_board_profile.h"
#include "millennium_nvram_model.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

std::filesystem::path emu_root() { return std::filesystem::path(COINLINE_EMU_SOURCE_DIR); }

bool load_text(std::filesystem::path const &p, std::string &out)
{
	std::ifstream in(p);
	if (!in)
		return false;
	out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
	return true;
}

} // namespace

int main()
{
	std::string board_txt;
	if (!load_text(emu_root() / "fixtures/board/board-profile-2line-vfd.json", board_txt)) {
		std::cerr << "cannot read board profile\n";
		return 1;
	}
	millennium_memory_layout_config layout{};
	std::string err;
	if (!millennium_board_parse_memory_layout(board_txt, layout, err)) {
		std::cerr << "memory layout: " << err << '\n';
		return 1;
	}

	std::string nv_txt;
	if (!load_text(emu_root() / "fixtures/nvram/factory-default.nvram.json", nv_txt)) {
		std::cerr << "cannot read factory nvram fixture\n";
		return 1;
	}

	millennium_nvram_model model{};
	model.configure(layout);
	if (!model.load_envelope_json(nv_txt, err)) {
		std::cerr << "load envelope: " << err << '\n';
		return 1;
	}
	if (!model.verify_checksum(err)) {
		std::cerr << "verify after factory load: " << err << '\n';
		return 1;
	}
	std::uint8_t const marker = 0x37;
	std::string werr;
	if (!model.write_nvram(64, marker, werr)) {
		std::cerr << "write: " << werr << '\n';
		return 1;
	}

	// Simulate machine reset: battery-backed RAM retains contents (reset_session is a no-op).
	model.reset_session();
	if (model.read_nvram(64) != marker) {
		std::cerr << "nvram did not survive reset_session\n";
		return 1;
	}

	std::vector<std::uint8_t> blob;
	model.serialize_state(blob);

	millennium_nvram_model round{};
	round.configure(layout);
	if (!round.deserialize_state(blob, err)) {
		std::cerr << "deserialize: " << err << '\n';
		return 1;
	}
	if (round.read_nvram(64) != marker) {
		std::cerr << "round-trip lost marker\n";
		return 1;
	}

	// Two consecutive "runs" via on-disk persistence (JSON envelope).
	auto const tmp = std::filesystem::temp_directory_path() / "coinline_nvram_persist_test.json";
	{
		std::string js;
		if (!model.save_envelope_json(js, err)) {
			std::cerr << "save json: " << err << '\n';
			return 1;
		}
		std::ofstream out(tmp, std::ios::binary);
		if (!out) {
			std::cerr << "cannot write temp nvram\n";
			return 1;
		}
		out << js;
	}
	millennium_nvram_model second{};
	second.configure(layout);
	{
		std::string js2;
		if (!load_text(tmp, js2)) {
			std::cerr << "cannot read temp nvram\n";
			return 1;
		}
		if (!second.load_envelope_json(js2, err)) {
			std::cerr << "reload envelope: " << err << '\n';
			return 1;
		}
	}
	if (second.read_nvram(64) != marker) {
		std::cerr << "second run did not see persisted byte\n";
		return 1;
	}
	std::filesystem::remove(tmp);

	// Deterministic factory image: 8192 zero bytes → sum8 0 (stable boot assumptions).
	{
		std::vector<std::uint8_t> z(8192, 0);
		if (millennium_nvram_model::compute_sum8(z) != 0) {
			std::cerr << "sum8 of zero buffer should be 0\n";
			return 1;
		}
	}

	return 0;
}
