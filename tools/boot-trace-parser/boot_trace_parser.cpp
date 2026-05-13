// SPDX-License-Identifier: GPL-2.0-or-later

#include "boot_trace_parser.h"

#include "../common/coinline_flat_json.hpp"

#include <cctype>
#include <cstdint>
#include <sstream>
#include <string_view>

namespace {

void trim_inplace(std::string &s)
{
	while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
		s.erase(s.begin());
	while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
		s.pop_back();
}

std::string format_table_line(std::string_view line)
{
	auto const ms = coinline_json_string_field(line, "milestone");
	if (!ms)
		return std::string("?: malformed milestone");

	if (*ms == "M0") {
		auto sha = coinline_json_string_field(line, "sha256");
		auto sz = coinline_json_raw_field(line, "size");
		std::ostringstream o;
		o << "M0 firmware loaded sha256=" << (sha ? *sha : "?") << " size=" << (sz ? *sz : "?");
		return o.str();
	}
	if (*ms == "M1") {
		auto pc = coinline_json_string_field(line, "pc");
		auto op = coinline_json_string_field(line, "opcode");
		std::ostringstream o;
		o << "M1 reset vector pc=" << (pc ? *pc : "?") << " opcode=" << (op ? *op : "?");
		return o.str();
	}
	if (*ms == "M2") {
		auto pc = coinline_json_string_field(line, "pc");
		std::ostringstream o;
		o << "M2 startup pc=" << (pc ? *pc : "?");
		return o.str();
	}
	if (*ms == "M3") {
		auto rw = coinline_json_raw_field(line, "ram_writes");
		auto sp = coinline_json_string_field(line, "sp");
		std::ostringstream o;
		o << "M3 ram_writes=" << (rw ? *rw : "?") << " sp=" << (sp ? *sp : "?");
		return o.str();
	}
	if (*ms == "M4") {
		std::string_view const lv = line;
		std::string regs_s = "?";
		auto const key = std::string_view("\"registers\":");
		auto p = lv.find(key);
		if (p != std::string_view::npos) {
			p += key.size();
			while (p < lv.size() && std::isspace(static_cast<unsigned char>(lv[p])))
				++p;
			if (p < lv.size() && lv[p] == '{') {
				int depth = 0;
				auto start = p;
				for (; p < lv.size(); ++p) {
					if (lv[p] == '{')
						++depth;
					else if (lv[p] == '}') {
						--depth;
						if (depth == 0) {
							regs_s = std::string(lv.substr(start, p - start + 1));
							break;
						}
					}
				}
			}
		}
		std::ostringstream o;
		o << "M4 registers " << regs_s;
		return o.str();
	}
	if (*ms == "M5") {
		auto dev = coinline_json_string_field(line, "device");
		auto pc = coinline_json_string_field(line, "pc");
		std::ostringstream o;
		o << "M5 first_io device=" << (dev ? *dev : "?") << " pc=" << (pc ? *pc : "?");
		return o.str();
	}
	if (*ms == "M6") {
		auto v = coinline_json_string_field(line, "vfd");
		std::ostringstream o;
		o << "M6 vfd=" << (v ? *v : "?");
		return o.str();
	}
	if (*ms == "M7") {
		auto k = coinline_json_raw_field(line, "keypad_scan_count");
		std::ostringstream o;
		o << "M7 keypad_scan_count=" << (k ? *k : "?");
		return o.str();
	}
	if (*ms == "M8") {
		auto a = coinline_json_string_field(line, "asci_state");
		auto dcd = coinline_json_raw_field(line, "dcd");
		auto cts = coinline_json_raw_field(line, "cts");
		std::ostringstream o;
		o << "M8 asci_state=" << (a ? *a : "?") << " dcd=" << (dcd ? *dcd : "?") << " cts=" << (cts ? *cts : "?");
		return o.str();
	}
	if (*ms == "M9") {
		auto spc = coinline_json_string_field(line, "scheduler_pc");
		std::ostringstream o;
		o << "M9 idle_gate scheduler_pc=" << (spc ? *spc : "?");
		return o.str();
	}
	if (*ms == "M10") {
		auto v = coinline_json_string_field(line, "vfd");
		auto idle = coinline_json_raw_field(line, "idle");
		std::ostringstream o;
		o << "M10 idle vfd=" << (v ? *v : "?") << " idle=" << (idle ? *idle : "?");
		return o.str();
	}
	std::ostringstream o;
	o << *ms << " (see boot-trace.jsonl)";
	return o.str();
}

bool line_is_object(std::string_view s)
{
	if (s.empty())
		return false;
	return s.front() == '{' && s.back() == '}';
}

} // namespace

bool boot_trace_parser_run(std::istream &in, std::ostream &out, BootTraceParserOptions const &opt, std::string &err)
{
	err.clear();
	std::string line;
	std::uint64_t line_no = 0;
	bool any = false;
	while (std::getline(in, line)) {
		++line_no;
		trim_inplace(line);
		if (line.empty())
			continue;
		if (!line_is_object(line)) {
			err = "invalid json: line " + std::to_string(line_no) + " (expected JSON object)";
			return false;
		}
		auto const ms = coinline_json_string_field(line, "milestone");
		if (!ms) {
			err = "invalid json: line " + std::to_string(line_no) + " (missing milestone)";
			return false;
		}
		if (!opt.filter_milestone.empty() && *ms != opt.filter_milestone)
			continue;

		if (opt.format == BootTraceOutputFormat::json) {
			out << line << '\n';
		} else {
			out << format_table_line(line) << '\n';
		}
		any = true;
	}
	if (!any && !opt.filter_milestone.empty()) {
		if (!opt.quiet)
			; // no output ok
	}
	(void)any;
	return true;
}
