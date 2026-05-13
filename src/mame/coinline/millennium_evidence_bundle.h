// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

/// Parameters for a minimal evidence bundle matching `specs/evidence-bundle.spec.md` layout.
struct millennium_evidence_bundle_params {
	std::string scenario_id = "boot-to-idle";
	std::string scenario_json;
	std::string board_profile_relpath = "fixtures/board/board-profile-2line-vfd.json";
	std::string boot_trace_jsonl_body;
	std::string vfd_final_json;
	bool deterministic_timestamps = false;
	std::string ts_start = "2000-01-01T00:00:00Z";
	std::string ts_end = "2000-01-01T00:00:01Z";
	std::string emulator_version = "coinline-emu";
	std::string emulator_commit = "local";
	std::string engine_track = "mame";
	std::string firmware_path = "../firmware/flash.bin";
	std::uint64_t firmware_size = 0;
	std::string firmware_sha256;
	std::string host_transport = "tcp";
	std::string host_endpoint = "127.0.0.1:9000";
	std::string result_status = "pass";
	std::string result_milestone = "M10";
	std::uint64_t elapsed_cycles = 0;
	std::string scenario_result_json;
	/// When true, emit mandatory audio artifacts under `audio/` per docs/audio-evidence-bundle-plan.md.
	bool expect_audio_traces = false;
	/// Preferred bodies when present (from wire `*_b64` or run-directory import). If empty, fall back to `*_stub_body`, then defaults.
	std::string audio_trace_jsonl_body;
	std::string voiceware_trace_jsonl_body;
	std::string supervision_trace_jsonl_body;
	std::string alerter_trace_jsonl_body;
	std::string audio_state_final_json_body;
	std::string io_trace_jsonl_body;
	std::string audio_trace_jsonl_stub_body;
	std::string voiceware_trace_jsonl_stub_body;
	/// Populated for Class C manifests — actual argv[0] or resolved path to `coinline-mame.exe` (see `docs/audio-evidence-bundle-plan.md`).
	std::string mame_executable;
	/// JSON array fragment, e.g. `["M6A","M6B"]`, written under `manifest.json` → `audio.audio_milestones_claimed`.
	std::string audio_milestones_claimed_json;
	/// JSON object mapping trace filenames to SHA-256 hex; may be empty `{}` until exporter fills hashes.
	std::string audio_trace_sha256_json;
};

bool millennium_evidence_bundle_write(std::filesystem::path const &root, millennium_evidence_bundle_params const &p,
	std::string &error_out);

/// Populate params from a coinline-mame run folder (trace JSONLs + optional evidence-summary.json). Sets `expect_audio_traces`.
bool millennium_evidence_bundle_fill_from_run_directory(std::filesystem::path const &run_dir,
	millennium_evidence_bundle_params &out, std::string &error_out);

/// Replace RFC3339 `ts_start` / `ts_end` fields with placeholders for byte-stable comparisons.
std::string millennium_evidence_manifest_normalize_timestamps(std::string const &manifest_json);
