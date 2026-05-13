// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <filesystem>
#include <istream>
#include <ostream>
#include <string>

enum class IoTraceOutputFormat { table, json };

struct IoTraceAnalyzerOptions {
	IoTraceOutputFormat format = IoTraceOutputFormat::table;
	std::string filter_port; // e.g. 0x60 — normalized internally
	std::string filter_rw;   // "r" or "w" or empty
	std::string filter_device; // e.g. vfd — requires port_map_path
	std::filesystem::path port_map_path;
	bool histogram = false;
	bool quiet = false;
};

bool io_trace_analyzer_run(std::istream &in, std::ostream &out, IoTraceAnalyzerOptions const &opt, std::string &err);
