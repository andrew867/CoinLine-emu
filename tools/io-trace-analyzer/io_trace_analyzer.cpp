// SPDX-License-Identifier: GPL-2.0-or-later

#include "io_trace_analyzer.h"

#include "../common/coinline_flat_json.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <optional>
#include <map>
#include <sstream>
#include <vector>

namespace {

void trim_inplace(std::string &s)
{
	while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
		s.erase(s.begin());
	while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
		s.pop_back();
}

std::string normalize_port_hex(std::string_view p)
{
	if (p.size() >= 2 && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
		std::string o = "0x";
		for (std::size_t i = 2; i < p.size(); ++i) {
			char c = p[i];
			if (std::isxdigit(static_cast<unsigned char>(c)))
				o.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
		}
		return o;
	}
	return std::string(p);
}

bool extract_port_device_pairs(std::string const &file_text, std::vector<std::pair<std::string, std::string>> &pairs,
	std::string &err)
{
	err.clear();
	pairs.clear();
	for (std::size_t pos = 0; pos < file_text.size();) {
		auto const key = file_text.find("\"port\"", pos);
		if (key == std::string::npos)
			break;
		std::size_t colon = file_text.find(':', key);
		if (colon == std::string::npos) {
			err = "invalid io-port-map.json (port key)";
			return false;
		}
		std::size_t i = colon + 1;
		while (i < file_text.size() && std::isspace(static_cast<unsigned char>(file_text[i])))
			++i;
		if (i >= file_text.size() || file_text[i] != '"') {
			err = "invalid io-port-map.json (port value quote)";
			return false;
		}
		++i;
		auto const endp = file_text.find('"', i);
		if (endp == std::string::npos) {
			err = "invalid io-port-map.json (port string)";
			return false;
		}
		std::string const port = normalize_port_hex(std::string_view(file_text.data() + i, endp - i));
		auto const obj_start = file_text.rfind('{', key);
		auto const obj_end = file_text.find('}', endp);
		if (obj_start == std::string::npos || obj_end == std::string::npos || obj_end < obj_start) {
			err = "invalid io-port-map.json (object bounds)";
			return false;
		}
		std::string const obj = file_text.substr(obj_start, obj_end - obj_start + 1);
		auto const dev = coinline_json_string_field(obj, "device");
		if (!dev) {
			err = "invalid io-port-map.json (missing device)";
			return false;
		}
		pairs.push_back({port, *dev});
		pos = obj_end + 1;
	}
	return true;
}

bool load_ports_for_device(std::filesystem::path const &map_path, std::string const &device,
	std::vector<std::string> &ports_out, std::string &err)
{
	std::ifstream f(map_path, std::ios::binary);
	if (!f) {
		err = "cannot open port map: " + map_path.string();
		return false;
	}
	std::ostringstream ss;
	ss << f.rdbuf();
	std::vector<std::pair<std::string, std::string>> pairs;
	if (!extract_port_device_pairs(ss.str(), pairs, err))
		return false;
	for (auto const &[port, dev] : pairs) {
		if (dev == device)
			ports_out.push_back(port);
	}
	if (ports_out.empty()) {
		err = "no ports for device: " + device;
		return false;
	}
	std::sort(ports_out.begin(), ports_out.end());
	ports_out.erase(std::unique(ports_out.begin(), ports_out.end()), ports_out.end());
	return true;
}

std::optional<std::uint16_t> parse_port_u16(std::string const &hex)
{
	try {
		std::string_view v(hex);
		if (v.size() >= 2 && v[0] == '0' && (v[1] == 'x' || v[1] == 'X'))
			v.remove_prefix(2);
		unsigned long x = std::stoul(std::string(v), nullptr, 16);
		if (x > 0xFFFF)
			return std::nullopt;
		return static_cast<std::uint16_t>(x);
	} catch (...) {
		return std::nullopt;
	}
}

std::string format_port_u16(std::uint16_t p)
{
	char buf[16];
	std::snprintf(buf, sizeof(buf), "0x%02X", unsigned(p));
	return buf;
}

std::string rw_from_dir(std::string_view dir)
{
	if (dir == "r")
		return "r";
	if (dir == "w")
		return "w";
	if (dir == "rw")
		return "rw";
	return std::string(dir);
}

bool line_is_object(std::string_view s)
{
	return !s.empty() && s.front() == '{' && s.back() == '}';
}

} // namespace

bool io_trace_analyzer_run(std::istream &in, std::ostream &out, IoTraceAnalyzerOptions const &opt, std::string &err)
{
	err.clear();
	std::vector<std::string> device_ports;
	if (!opt.filter_device.empty()) {
		if (opt.port_map_path.empty()) {
			err = "--device requires --port-map <path> (e.g. fixtures/board/io-port-map.json)";
			return false;
		}
		if (!load_ports_for_device(opt.port_map_path, opt.filter_device, device_ports, err))
			return false;
	}

	std::string const want_port = opt.filter_port.empty() ? std::string() : normalize_port_hex(opt.filter_port);

	std::string line;
	std::uint64_t line_no = 0;
	std::map<std::uint16_t, std::uint64_t> hist;

	std::vector<std::string> kept_lines;
	while (std::getline(in, line)) {
		++line_no;
		trim_inplace(line);
		if (line.empty())
			continue;
		if (!line_is_object(line)) {
			err = "invalid io-trace line " + std::to_string(line_no) + " (expected JSON object)";
			return false;
		}
		auto const port = coinline_json_string_field(line, "port");
		if (!port) {
			err = "invalid io-trace line " + std::to_string(line_no) + " (missing port)";
			return false;
		}
		std::string const pnorm = normalize_port_hex(*port);

		if (!want_port.empty() && pnorm != want_port)
			continue;

		if (!opt.filter_device.empty()) {
			bool match = false;
			for (auto const &dp : device_ports) {
				if (dp == pnorm) {
					match = true;
					break;
				}
			}
			if (!match)
				continue;
		}

		std::string rw;
		if (auto d = coinline_json_string_field(line, "dir"))
			rw = rw_from_dir(*d);
		else if (auto r = coinline_json_string_field(line, "rw"))
			rw = *r;
		if (!opt.filter_rw.empty()) {
			if (rw != opt.filter_rw)
				continue;
		}

		auto const pu = parse_port_u16(pnorm);
		if (!pu) {
			err = "invalid port on line " + std::to_string(line_no) + ": " + pnorm;
			return false;
		}
		++hist[*pu];
		if (!opt.histogram)
			kept_lines.push_back(line);
	}

	if (opt.histogram) {
		if (opt.format == IoTraceOutputFormat::json) {
			out << "{\"histogram\":[";
			bool first = true;
			for (auto const &[p, c] : hist) {
				if (!first)
					out << ',';
				first = false;
				out << "{\"port\":\"" << format_port_u16(p) << "\",\"count\":" << c << '}';
			}
			out << "]}\n";
		} else {
			out << "port\tcount\n";
			for (auto const &[p, c] : hist)
				out << format_port_u16(p) << '\t' << c << '\n';
		}
		return true;
	}

	for (auto const &l : kept_lines) {
		if (opt.format == IoTraceOutputFormat::json)
			out << l << '\n';
		else {
			auto const port = coinline_json_string_field(l, "port");
			auto const cyc = coinline_json_raw_field(l, "cycle");
			auto const dir = coinline_json_string_field(l, "dir");
			auto const val = coinline_json_string_field(l, "value");
			out << (port ? *port : "?") << '\t' << (cyc ? *cyc : "?") << '\t' << (dir ? *dir : "?") << '\t'
			    << (val ? *val : "?") << '\n';
		}
	}
	return true;
}
