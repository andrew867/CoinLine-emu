// SPDX-License-Identifier: GPL-2.0-or-later

#include "boot_trace_parser.h"

#include <fstream>
#include <iostream>
#include <string>

namespace {

void print_help_short()
{
	std::cout << "boot-trace-parser — summarize boot-trace.jsonl from evidence bundles\n"
		  << "\n"
		  << "Usage: boot-trace-parser [--input <path>] [--output <path>] [--filter <M#>] [--format table|json] [--quiet]\n"
		  << "\n"
		  << "Examples:\n"
		  << "  boot-trace-parser --input out/boot/boot-trace.jsonl\n"
		  << "  boot-trace-parser --input out/boot/boot-trace.jsonl --filter M5\n"
		  << "\n"
		  << "Use --help-all for every flag.\n";
}

void print_help_all()
{
	print_help_short();
	std::cout << "\nFlags:\n"
		  << "  --input <path>     JSONL input (default: stdin)\n"
		  << "  --output <path>    Write here (default: stdout)\n"
		  << "  --filter <M#>      Only lines for this milestone (e.g. M10)\n"
		  << "  --format table|json  Output style (default: table)\n"
		  << "  --quiet            Suppress non-error stderr\n"
		  << "  --help, -h         Short help\n"
		  << "  --help-all         This text\n";
}

} // namespace

int main(int argc, char **argv)
{
	if (argc <= 1) {
		print_help_short();
		std::cerr << "\nerror: no arguments (use --help)\n";
		return 1;
	}
	BootTraceParserOptions opt{};
	std::string in_path, out_path;
	for (int i = 1; i < argc; ++i) {
		std::string const a = argv[i];
		if (a == "--help" || a == "-h") {
			print_help_short();
			return 0;
		}
		if (a == "--help-all") {
			print_help_all();
			return 0;
		}
		if (a == "--quiet") {
			opt.quiet = true;
			continue;
		}
		if (a == "--input" && i + 1 < argc)
			in_path = argv[++i];
		else if (a == "--output" && i + 1 < argc)
			out_path = argv[++i];
		else if (a == "--filter" && i + 1 < argc)
			opt.filter_milestone = argv[++i];
		else if (a == "--format" && i + 1 < argc) {
			std::string const f = argv[++i];
			if (f == "json")
				opt.format = BootTraceOutputFormat::json;
			else if (f == "table")
				opt.format = BootTraceOutputFormat::table;
			else {
				std::cerr << "error: unknown --format (use table or json)\n";
				return 1;
			}
		} else {
			std::cerr << "error: unknown argument: " << a << "\n";
			std::cerr << "Try boot-trace-parser --help\n";
			return 1;
		}
	}

	std::ifstream in_file;
	std::istream *in = &std::cin;
	if (!in_path.empty()) {
		in_file.open(in_path, std::ios::binary);
		if (!in_file) {
			std::cerr << "error: cannot open input: " << in_path << '\n';
			return 1;
		}
		in = &in_file;
	}

	std::ofstream out_file;
	std::ostream *out = &std::cout;
	if (!out_path.empty()) {
		out_file.open(out_path, std::ios::binary);
		if (!out_file) {
			std::cerr << "error: cannot open output: " << out_path << '\n';
			return 1;
		}
		out = &out_file;
	}

	std::string err;
	if (!boot_trace_parser_run(*in, *out, opt, err)) {
		std::cerr << "error: " << err << '\n';
		return 1;
	}
	return 0;
}
