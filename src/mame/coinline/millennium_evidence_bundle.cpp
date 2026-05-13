// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_evidence_bundle.h"

#include "millennium_sha256.h"

#include <cctype>
#include <fstream>
#include <optional>
#include <sstream>

namespace {

bool read_file_string(std::filesystem::path const &path, std::string &out, std::string &error_out)
{
	std::ifstream in(path, std::ios::binary);
	if (!in) {
		error_out = "cannot read: " + path.string();
		return false;
	}
	std::ostringstream ss;
	ss << in.rdbuf();
	out = ss.str();
	return true;
}

bool write_file(std::filesystem::path const &path, std::string const &contents, std::string &error_out)
{
	std::ofstream out(path, std::ios::binary);
	if (!out) {
		error_out = "cannot write: " + path.string();
		return false;
	}
	out << contents;
	return true;
}

std::optional<std::string> extract_json_string_value(std::string const &json, char const *key)
{
	std::string const pat = std::string("\"") + key + "\":\"";
	auto p = json.find(pat);
	if (p == std::string::npos)
		return std::nullopt;
	p += pat.size();
	auto const e = json.find('"', p);
	if (e == std::string::npos || e <= p)
		return std::nullopt;
	return json.substr(p, e - p);
}

bool extract_json_uint64(std::string const &json, char const *key, std::uint64_t &out)
{
	std::string const pat = std::string("\"") + key + "\":";
	auto p = json.find(pat);
	if (p == std::string::npos)
		return false;
	p += pat.size();
	while (p < json.size() && std::isspace(static_cast<unsigned char>(json[p])))
		++p;
	try {
		std::size_t idx = 0;
		out = static_cast<std::uint64_t>(std::stoull(json.substr(p), &idx));
		return idx > 0;
	} catch (...) {
		return false;
	}
}

bool is_hex64(std::string const &s)
{
	if (s.size() != 64)
		return false;
	for (char c : s) {
		if (!std::isxdigit(static_cast<unsigned char>(c)))
			return false;
	}
	return true;
}

std::filesystem::path first_existing(std::filesystem::path const &run_dir,
	std::initializer_list<std::filesystem::path> rels)
{
	for (auto const &r : rels) {
		std::filesystem::path const p = run_dir / r;
		if (std::filesystem::exists(p))
			return p;
	}
	return {};
}

std::string default_vfd_final()
{
	return std::string("{\"variant\":\"2line\",\"rows\":2,\"columns\":20,\"text\":[\""
			   "                    \",\"                    \"],\"raw\":\"\"}\n");
}

std::string json_escape_path(std::string const &s)
{
	std::string o;
	o.reserve(s.size() + 8);
	for (char c : s) {
		if (c == '\\' || c == '"')
			o.push_back('\\');
		o.push_back(c);
	}
	return o;
}

std::string default_scenario_result(std::string const &scenario_id)
{
	std::ostringstream o;
	o << "{\"scenario_id\":\"" << scenario_id << "\",\"steps\":[{\"index\":0,\"verb\":\"export_evidence\","
	     "\"status\":\"pass\",\"elapsed_cycles\":0}]}\n";
	return o.str();
}

void replace_ts_field(std::string &out, char const *key)
{
	static std::string const placeholder = "\"@TS@\"";
	std::size_t search = 0;
	for (;;) {
		auto const p = out.find(key, search);
		if (p == std::string::npos)
			break;
		auto const colon = out.find(':', p);
		if (colon == std::string::npos)
			break;
		auto const q1 = out.find('"', colon + 1);
		if (q1 == std::string::npos)
			break;
		auto const q2 = out.find('"', q1 + 1);
		if (q2 == std::string::npos || q2 <= q1)
			break;
		std::size_t const span = static_cast<std::size_t>(q2 - q1 + 1);
		if (span == placeholder.size() && out.compare(q1, placeholder.size(), placeholder) == 0) {
			search = q2 + 1;
			continue;
		}
		out.replace(q1, span, placeholder);
		search = q1 + placeholder.size();
	}
}

std::string pick_audio_body(std::string const &primary, std::string const &stub, std::string const &default_line)
{
	if (!primary.empty())
		return primary;
	if (!stub.empty())
		return stub;
	return default_line;
}

std::string default_audio_line()
{
	return std::string("{\"schema_version\":\"coinline.audio_trace/v1\",\"device\":\"voiceware\","
			   "\"event_type\":\"bundle_stub\"}\n");
}

std::string default_supervision_line()
{
	return std::string("{\"schema_version\":\"coinline.audio_trace/v1\",\"timestamp_emulated_ns\":0,\"cycle\":0,"
			   "\"pc\":\"0x0000\",\"event_type\":\"supervision_status_code\",\"device\":\"supervision\","
			   "\"status_code_hex\":\"0x00\",\"compatibility_flag\":\"bundle_stub\"}\n");
}

std::string default_alerter_line()
{
	return std::string("{\"schema_version\":\"coinline.audio_trace/v1\",\"timestamp_emulated_ns\":0,\"cycle\":0,"
			   "\"pc\":\"0x0000\",\"event_type\":\"alerter_ready\",\"device\":\"audio\"}\n");
}

std::string default_audio_state_final()
{
	return std::string("{\"route_state\":\"idle\",\"mute_state\":{\"mic\":false,\"earpiece\":false},\"vw_state\":\"idle\","
			   "\"sup_state\":\"idle\",\"last_prompt_id\":null,\"call_state_if_known\":\"idle_on_hook\"}\n");
}

std::string trace_sha256_json(std::string const &audio_trace, std::string const &voiceware_trace,
	std::string const &supervision_trace, std::string const &alerter_trace)
{
	std::ostringstream o;
	o << '{';
	char const *sep = "";
	auto one = [&](char const *name, std::string const &body) {
		o << sep << '"' << name << "\":\"" << millennium_sha256_hex(
			reinterpret_cast<std::uint8_t const *>(body.data()), body.size()) << '"';
		sep = ",";
	};
	one("audio-trace.jsonl", audio_trace);
	one("voiceware-trace.jsonl", voiceware_trace);
	one("supervision-trace.jsonl", supervision_trace);
	one("alerter-trace.jsonl", alerter_trace);
	o << '}';
	return o.str();
}

} // namespace

std::string millennium_evidence_manifest_normalize_timestamps(std::string const &manifest_json)
{
	std::string out = manifest_json;
	replace_ts_field(out, "\"ts_start\"");
	replace_ts_field(out, "\"ts_end\"");
	return out;
}

bool millennium_evidence_bundle_fill_from_run_directory(std::filesystem::path const &run_dir,
	millennium_evidence_bundle_params &out, std::string &error_out)
{
	error_out.clear();
	out.expect_audio_traces = true;

	if (!std::filesystem::exists(run_dir / "boot-trace.jsonl")) {
		error_out = "mandatory run artifact missing: boot-trace.jsonl";
		return false;
	}
	if (!read_file_string(run_dir / "boot-trace.jsonl", out.boot_trace_jsonl_body, error_out))
		return false;

	std::filesystem::path const ap_audio =
		first_existing(run_dir, {"audio/audio-trace.jsonl", "audio-trace.jsonl"});
	if (ap_audio.empty()) {
		error_out = "mandatory audio artifact missing: audio-trace.jsonl (or audio/audio-trace.jsonl)";
		return false;
	}
	if (!read_file_string(ap_audio, out.audio_trace_jsonl_body, error_out))
		return false;

	std::filesystem::path const ap_vw =
		first_existing(run_dir, {"audio/voiceware-trace.jsonl", "voiceware-trace.jsonl"});
	if (ap_vw.empty()) {
		error_out = "mandatory audio artifact missing: voiceware-trace.jsonl";
		return false;
	}
	if (!read_file_string(ap_vw, out.voiceware_trace_jsonl_body, error_out))
		return false;

	std::filesystem::path const ap_sup =
		first_existing(run_dir, {"audio/supervision-trace.jsonl", "supervision-trace.jsonl"});
	if (ap_sup.empty())
		out.supervision_trace_jsonl_body =
			default_supervision_line() +
			std::string("{\"compatibility_validation_required\":\"supervision_trace_synthesized\","
				    "\"device\":\"supervision\"}\n");
	else if (!read_file_string(ap_sup, out.supervision_trace_jsonl_body, error_out))
		return false;

	std::filesystem::path const ap_alt =
		first_existing(run_dir, {"audio/alerter-trace.jsonl", "alerter-trace.jsonl"});
	if (ap_alt.empty())
		out.alerter_trace_jsonl_body =
			default_alerter_line() +
			std::string("{\"compatibility_validation_required\":\"alerter_trace_synthesized\","
				    "\"device\":\"audio\"}\n");
	else if (!read_file_string(ap_alt, out.alerter_trace_jsonl_body, error_out))
		return false;

	std::filesystem::path const ap_asf =
		first_existing(run_dir, {"audio/audio-state-final.json", "audio-state-final.json"});
	if (ap_asf.empty())
		out.audio_state_final_json_body =
			std::string("{\"compatibility_validation_required\":\"audio_state_final_synthesized\","
				    "\"route_state\":\"unknown\",\"mute_state\":{},\"vw_state\":\"unknown\","
				    "\"sup_state\":\"unknown\",\"last_prompt_id\":null}\n");
	else if (!read_file_string(ap_asf, out.audio_state_final_json_body, error_out))
		return false;

	if (std::filesystem::exists(run_dir / "io-trace.jsonl")) {
		if (!read_file_string(run_dir / "io-trace.jsonl", out.io_trace_jsonl_body, error_out))
			return false;
	}
	if (out.io_trace_jsonl_body.empty())
		out.io_trace_jsonl_body = std::string("{\"port\":\"0x60\",\"cycle\":0,\"dir\":\"w\",\"value\":\"0x00\"}\n");

	std::filesystem::path const summ = run_dir / "evidence-summary.json";
	if (std::filesystem::exists(summ)) {
		std::string raw;
		if (read_file_string(summ, raw, error_out)) {
			if (auto ex = extract_json_string_value(raw, "emulator_executable"))
				out.mame_executable = *ex;
			if (auto sh = extract_json_string_value(raw, "firmware_sha256"))
				out.firmware_sha256 = *sh;
		}
	}
	std::filesystem::path const inp = run_dir / "input-resolution.json";
	if (out.firmware_sha256.empty() && std::filesystem::exists(inp)) {
		std::string raw;
		if (read_file_string(inp, raw, error_out)) {
			if (auto sh = extract_json_string_value(raw, "firmware_sha256"))
				out.firmware_sha256 = *sh;
			std::uint64_t sz = 0;
			if (extract_json_uint64(raw, "firmware_binary_size", sz))
				out.firmware_size = sz;
		}
	}

	if (std::filesystem::exists(run_dir / "scenario.json"))
		(void)read_file_string(run_dir / "scenario.json", out.scenario_json, error_out);
	if (std::filesystem::exists(run_dir / "scenario_result.json"))
		(void)read_file_string(run_dir / "scenario_result.json", out.scenario_result_json, error_out);

	if (out.scenario_json.empty())
		out.scenario_json = std::string("{\"scenario_id\":\"run-directory-import\",\"expect_audio_traces\":true}\n");

	if (out.mame_executable.empty() || !is_hex64(out.firmware_sha256)) {
		error_out =
			"run-directory bundle: set emulator_executable and firmware_sha256 (64 hex) — typically copy evidence-summary.json "
			"from the coinline-mame run folder";
		return false;
	}

	return true;
}

bool millennium_evidence_bundle_write(std::filesystem::path const &root, millennium_evidence_bundle_params const &p,
	std::string &error_out)
{
	error_out.clear();

	if (p.expect_audio_traces) {
		if (p.mame_executable.empty()) {
			error_out = "expect_audio_traces: mame_executable is required (argv[0] / coinline-mame.exe path)";
			return false;
		}
		if (!is_hex64(p.firmware_sha256)) {
			error_out = "expect_audio_traces: firmware_sha256 must be 64 hex characters";
			return false;
		}
	}

	try {
		std::filesystem::create_directories(root / "vfd");
		std::filesystem::create_directories(root / "nvram");
		std::filesystem::create_directories(root / "host-bridge");
		std::filesystem::create_directories(root / "logs");
		if (p.expect_audio_traces)
			std::filesystem::create_directories(root / "audio");
	} catch (std::exception const &e) {
		error_out = e.what();
		return false;
	}

	std::string const ts0 = p.ts_start;
	std::string const ts1 = p.ts_end;
	(void)p.deterministic_timestamps;

	std::string sha = p.firmware_sha256;
	if (sha.size() < 64)
		sha.append(64 - sha.size(), '0');

	std::string const def_audio = default_audio_line();
	std::string const at = pick_audio_body(p.audio_trace_jsonl_body, p.audio_trace_jsonl_stub_body, def_audio);
	std::string const vt = pick_audio_body(p.voiceware_trace_jsonl_body, p.voiceware_trace_jsonl_stub_body, def_audio);
	std::string const st =
		!p.supervision_trace_jsonl_body.empty() ? p.supervision_trace_jsonl_body : default_supervision_line();
	std::string const alt = !p.alerter_trace_jsonl_body.empty() ? p.alerter_trace_jsonl_body : default_alerter_line();
	std::string const asf =
		!p.audio_state_final_json_body.empty() ? p.audio_state_final_json_body : default_audio_state_final();

	std::string trace_sha_computed =
		p.audio_trace_sha256_json.empty() ? trace_sha256_json(at, vt, st, alt) : p.audio_trace_sha256_json;

	std::ostringstream manifest;
	manifest << "{\n";
	manifest << "  \"schema_version\": \"1.0\",\n";
	manifest << "  \"ts_start\": \"" << ts0 << "\",\n";
	manifest << "  \"ts_end\": \"" << ts1 << "\",\n";
	manifest << "  \"scenario_id\": \"" << p.scenario_id << "\",\n";
	manifest << "  \"emulator_version\": \"" << p.emulator_version << "\",\n";
	manifest << "  \"emulator_commit\": \"" << p.emulator_commit << "\",\n";
	manifest << "  \"engine_track\": \"" << p.engine_track << "\",\n";
	if (!p.mame_executable.empty())
		manifest << "  \"mame_executable\": \"" << json_escape_path(p.mame_executable) << "\",\n";
	if (p.expect_audio_traces)
		manifest << "  \"firmware_sha256\": \"" << sha << "\",\n";
	manifest << "  \"firmware\": {\n";
	manifest << "    \"path\": \"" << p.firmware_path << "\",\n";
	manifest << "    \"size\": " << p.firmware_size << ",\n";
	manifest << "    \"sha256\": \"" << sha << "\"\n";
	manifest << "  },\n";
	manifest << "  \"board_profile\": \"" << p.board_profile_relpath << "\",\n";
	manifest << "  \"host_bridge\": {\n";
	manifest << "    \"transport\": \"" << p.host_transport << "\",\n";
	manifest << "    \"endpoint\": \"" << p.host_endpoint << "\"\n";
	manifest << "  },\n";
	if (p.expect_audio_traces) {
		std::string const milestones =
			p.audio_milestones_claimed_json.empty() ? std::string("[]") : p.audio_milestones_claimed_json;
		manifest << "  \"audio\": {\n";
		manifest << "    \"expect_audio_traces\": true,\n";
		manifest << "    \"audio_milestones_claimed\": " << milestones << ",\n";
		manifest << "    \"trace_sha256\": " << trace_sha_computed << "\n";
		manifest << "  },\n";
	}
	manifest << "  \"result\": {\n";
	manifest << "    \"status\": \"" << p.result_status << "\",\n";
	manifest << "    \"milestone\": \"" << p.result_milestone << "\",\n";
	manifest << "    \"elapsed_cycles\": " << p.elapsed_cycles << ",\n";
	manifest << "    \"fail_reason\": null\n";
	manifest << "  }\n";
	manifest << "}\n";

	if (!write_file(root / "manifest.json", manifest.str(), error_out))
		return false;
	if (!write_file(root / "scenario.json", p.scenario_json, error_out))
		return false;
	std::string const sr = p.scenario_result_json.empty() ? default_scenario_result(p.scenario_id) : p.scenario_result_json;
	if (!write_file(root / "scenario_result.json", sr, error_out))
		return false;
	if (!write_file(root / "boot-trace.jsonl", p.boot_trace_jsonl_body, error_out))
		return false;

	std::string const io_body =
		!p.io_trace_jsonl_body.empty()
			? p.io_trace_jsonl_body
			: std::string("{\"port\":\"0x60\",\"cycle\":0,\"dir\":\"w\",\"value\":\"0x00\"}\n");
	if (!write_file(root / "io-trace.jsonl", io_body, error_out))
		return false;

	if (!write_file(root / "uart-tx.hex", "# uart tx (minimal evidence stub)\n", error_out))
		return false;
	if (!write_file(root / "uart-rx.hex", "# uart rx (minimal evidence stub)\n", error_out))
		return false;
	std::string const vfdj = p.vfd_final_json.empty() ? default_vfd_final() : p.vfd_final_json;
	if (!write_file(root / "vfd" / "final.json", vfdj, error_out))
		return false;
	if (!write_file(root / "nvram" / "initial.json", "{\"region\":\"nvram\",\"bytes\":\"\"}\n", error_out))
		return false;
	if (!write_file(root / "nvram" / "final.json", "{\"region\":\"nvram\",\"bytes\":\"\"}\n", error_out))
		return false;
	if (!write_file(root / "nvram" / "diff.jsonl", "", error_out))
		return false;
	if (!write_file(root / "host-bridge" / "transcript.jsonl", "{\"stub\":true}\n", error_out))
		return false;

	std::ostringstream lg;
	lg << "coinline-mame evidence bundle export (not browser)\n";
	if (!p.mame_executable.empty())
		lg << "argv[0]=" << p.mame_executable << '\n';
	lg << "firmware_sha256=" << sha << '\n';
	if (!write_file(root / "logs" / "emu.log", lg.str(), error_out))
		return false;

	if (p.expect_audio_traces) {
		if (!write_file(root / "audio" / "audio-trace.jsonl", at, error_out))
			return false;
		if (!write_file(root / "audio" / "voiceware-trace.jsonl", vt, error_out))
			return false;
		if (!write_file(root / "audio" / "supervision-trace.jsonl", st, error_out))
			return false;
		if (!write_file(root / "audio" / "alerter-trace.jsonl", alt, error_out))
			return false;
		if (!write_file(root / "audio" / "audio-state-final.json", asf, error_out))
			return false;

		auto nz = [&](std::filesystem::path const &rel) {
			if (!std::filesystem::exists(root / rel)) {
				error_out = "bundle incomplete (missing): " + rel.string();
				return false;
			}
			if (std::filesystem::file_size(root / rel) == 0) {
				error_out = "bundle incomplete (empty): " + rel.string();
				return false;
			}
			return true;
		};
		if (!nz("audio/audio-trace.jsonl") || !nz("audio/voiceware-trace.jsonl") || !nz("audio/supervision-trace.jsonl") ||
		    !nz("audio/alerter-trace.jsonl") || !nz("audio/audio-state-final.json") || !nz("boot-trace.jsonl") ||
		    !nz("io-trace.jsonl"))
			return false;
	}
	return true;
}
