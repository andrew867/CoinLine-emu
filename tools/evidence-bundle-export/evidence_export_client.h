// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "millennium_evidence_bundle.h"

#include <filesystem>
#include <string>

/// TCP wire format: first 4 bytes big-endian payload length, then UTF-8 JSON object (see README).
/// JSON uses `*_b64` fields for large blobs. Does not embed host filesystem firmware paths in output.
bool evidence_export_parse_wire_json(std::string const &json, millennium_evidence_bundle_params &out, std::string &err);

/// Connect to `host` / `port` (TCP), read framed JSON payload into `payload_out`.
bool evidence_export_recv_payload(std::string const &host, std::string const &port, std::string &payload_out, std::string &err);

/// Full flow: receive wire JSON, write evidence bundle to `out_dir`. Optional `scenario_id_override` replaces scenario_id after parse.
bool evidence_bundle_export_run(std::string const &emulator_host_port, std::filesystem::path const &out_dir,
	std::string const &scenario_id_override, std::string &err);

/// Build a bundle from a `coinline-mame` / `run-screenshot-capture` output directory (traces on disk, not a browser).
bool evidence_bundle_export_from_run_directory(std::filesystem::path const &run_dir, std::filesystem::path const &out_dir,
	std::string const &scenario_id_override, std::string &err);
