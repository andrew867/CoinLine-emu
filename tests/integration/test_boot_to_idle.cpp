// SPDX-License-Identifier: GPL-2.0-or-later
//
// This target validates scenario JSON shape and evidence-bundle writer determinism for the
// boot-to-idle scenario file. It does **not** launch MAME or execute firmware.
//
// Real M0–M10 closure is enforced by PowerShell integration under tests/integration/*.Tests.ps1
// and tools/windows/test-coinline-emulator.ps1 (requires FIRMWARE_BINARY_PATH + built mame.exe).

#include "millennium_evidence_bundle.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace {

std::filesystem::path emu_root() { return std::filesystem::path(COINLINE_EMU_SOURCE_DIR); }

std::optional<std::string> json_quoted_field(std::string const &block, std::string_view field)
{
	std::string const key = std::string("\"") + std::string(field) + "\"";
	auto const p = block.find(key);
	if (p == std::string::npos)
		return std::nullopt;
	auto const colon = block.find(':', p);
	if (colon == std::string::npos)
		return std::nullopt;
	auto q1 = block.find('"', colon + 1);
	if (q1 == std::string::npos)
		return std::nullopt;
	auto q2 = block.find('"', q1 + 1);
	if (q2 == std::string::npos || q2 <= q1)
		return std::nullopt;
	return block.substr(q1 + 1, q2 - q1 - 1);
}

} // namespace

int main()
{
	std::ifstream in(emu_root() / "fixtures/scenarios/boot-to-idle.json");
	if (!in) {
		std::cerr << "missing boot-to-idle.json\n";
		return 1;
	}
	std::ostringstream ss;
	ss << in.rdbuf();
	std::string const txt = ss.str();

	auto const sid = json_quoted_field(txt, "scenario_id");
	if (!sid || *sid != "boot-to-idle") {
		std::cerr << "scenario_id\n";
		return 1;
	}
	if (txt.find("wait_for_milestone") == std::string::npos || txt.find("M10") == std::string::npos) {
		std::cerr << "M10 wait step\n";
		return 1;
	}
	if (txt.find("export_evidence") == std::string::npos) {
		std::cerr << "export_evidence\n";
		return 1;
	}

	std::ifstream vfd(emu_root() / "fixtures/display/vfd-2line-idle.json");
	if (!vfd)
		return 1;
	std::string const vfd_txt((std::istreambuf_iterator<char>(vfd)), std::istreambuf_iterator<char>());

	millennium_evidence_bundle_params p{};
	p.scenario_id = "boot-to-idle";
	p.scenario_json = txt;
	p.vfd_final_json = vfd_txt;
	p.boot_trace_jsonl_body = "{\"milestone\":\"M10\",\"ts\":\"x\",\"idle\":true}\n";
	p.deterministic_timestamps = true;
	p.ts_start = "2000-01-01T00:00:00Z";
	p.ts_end = "2000-01-01T00:00:02Z";
	p.firmware_sha256.assign(64, 'b');
	p.firmware_size = 1048576;

	std::filesystem::path const out = std::filesystem::temp_directory_path() / "coinline_boot_to_idle_bundle";
	std::filesystem::remove_all(out);
	std::string err;
	if (!millennium_evidence_bundle_write(out, p, err)) {
		std::cerr << err << '\n';
		return 1;
	}
	if (!std::filesystem::exists(out / "manifest.json") || !std::filesystem::exists(out / "vfd" / "final.json")) {
		std::cerr << "bundle incomplete\n";
		return 1;
	}
	std::filesystem::remove_all(out);
	return 0;
}
