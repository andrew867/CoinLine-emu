// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <istream>
#include <ostream>
#include <string>

enum class BootTraceOutputFormat { table, json };

struct BootTraceParserOptions {
	BootTraceOutputFormat format = BootTraceOutputFormat::table;
	std::string filter_milestone; // empty = all
	bool quiet = false;
};

bool boot_trace_parser_run(std::istream &in, std::ostream &out, BootTraceParserOptions const &opt, std::string &err);
