// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_evidence_bundle.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::filesystem::path emu_root() { return std::filesystem::path(COINLINE_EMU_SOURCE_DIR); }

bool read_all(std::filesystem::path const &p, std::string &out)
{
	std::ifstream in(p, std::ios::binary);
	if (!in)
		return false;
	std::ostringstream ss;
	ss << in.rdbuf();
	out = ss.str();
	return true;
}

bool compare_bundles(std::filesystem::path const &a, std::filesystem::path const &b)
{
	std::vector<std::filesystem::path> rel;
	for (auto const &e : std::filesystem::recursive_directory_iterator(a)) {
		if (!e.is_regular_file())
			continue;
		std::filesystem::path const r = std::filesystem::relative(e.path(), a);
		rel.push_back(r);
	}
	std::sort(rel.begin(), rel.end());
	std::vector<std::filesystem::path> relb;
	for (auto const &e : std::filesystem::recursive_directory_iterator(b)) {
		if (!e.is_regular_file())
			continue;
		relb.push_back(std::filesystem::relative(e.path(), b));
	}
	std::sort(relb.begin(), relb.end());
	if (rel != relb) {
		std::cerr << "bundle file sets differ\n";
		return false;
	}
	for (auto const &r : rel) {
		std::string ca, cb;
		if (!read_all(a / r, ca) || !read_all(b / r, cb))
			return false;
		if (r.filename() == "manifest.json") {
			ca = millennium_evidence_manifest_normalize_timestamps(ca);
			cb = millennium_evidence_manifest_normalize_timestamps(cb);
		}
		if (ca != cb) {
			std::cerr << "mismatch " << r.string() << '\n';
			return false;
		}
	}
	return true;
}

} // namespace

int main()
{
	std::ifstream scen(emu_root() / "fixtures/scenarios/boot-to-idle.json");
	if (!scen)
		return 1;
	std::string const scenario_txt((std::istreambuf_iterator<char>(scen)), std::istreambuf_iterator<char>());

	std::ifstream vfd(emu_root() / "fixtures/display/vfd-2line-idle.json");
	if (!vfd)
		return 1;
	std::string const vfd_txt((std::istreambuf_iterator<char>(vfd)), std::istreambuf_iterator<char>());

	millennium_evidence_bundle_params p{};
	p.scenario_id = "boot-to-idle";
	p.scenario_json = scenario_txt;
	p.board_profile_relpath = "fixtures/board/board-profile-2line-vfd.json";
	p.boot_trace_jsonl_body =
		"{\"milestone\":\"M0\",\"ts\":\"t0\",\"idle\":false}\n"
		"{\"milestone\":\"M10\",\"ts\":\"t10\",\"idle\":true}\n";
	p.vfd_final_json = vfd_txt;
	p.deterministic_timestamps = true;
	p.ts_start = "2000-01-01T00:00:00Z";
	p.ts_end = "2000-01-01T00:00:01Z";
	p.firmware_size = 1048576;
	p.firmware_sha256.assign(64, 'a');
	p.result_milestone = "M10";

	std::filesystem::path const t1 = std::filesystem::temp_directory_path() / "coinline_evidence_a";
	std::filesystem::path const t2 = std::filesystem::temp_directory_path() / "coinline_evidence_b";
	std::filesystem::remove_all(t1);
	std::filesystem::remove_all(t2);

	std::string err;
	if (!millennium_evidence_bundle_write(t1, p, err)) {
		std::cerr << err << '\n';
		return 1;
	}
	if (!millennium_evidence_bundle_write(t2, p, err)) {
		std::cerr << err << '\n';
		return 1;
	}
	if (!compare_bundles(t1, t2)) {
		std::cerr << "determinism compare failed\n";
		return 1;
	}
	std::filesystem::remove_all(t1);
	std::filesystem::remove_all(t2);
	return 0;
}
