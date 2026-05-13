// SPDX-License-Identifier: GPL-2.0-or-later

#include "boot_trace_parser.h"

#include <fstream>
#include <sstream>
#include <string>

#ifndef COINLINE_EMU_SOURCE_DIR
#error COINLINE_EMU_SOURCE_DIR
#endif

int main()
{
	std::string const path = std::string(COINLINE_EMU_SOURCE_DIR) + "/tools/boot-trace-parser/tests/fixtures/sample-boot-trace.jsonl";
	std::ifstream in(path);
	if (!in)
		return 1;
	std::ostringstream out;
	std::string err;
	BootTraceParserOptions opt{};
	if (!boot_trace_parser_run(in, out, opt, err)) {
		return 1;
	}
	std::string const expected =
		"M0 firmware loaded sha256=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa size=1048576\n"
		"M1 reset vector pc=0x0000 opcode=0xED\n"
		"M10 idle vfd=idle_ok idle=true\n";
	if (out.str() != expected)
		return 1;

	std::ifstream in2(path);
	std::ostringstream out2;
	opt.filter_milestone = "M10";
	if (!boot_trace_parser_run(in2, out2, opt, err))
		return 1;
	if (out2.str() != "M10 idle vfd=idle_ok idle=true\n")
		return 1;

	std::istringstream bad("{not-json\n");
	std::ostringstream discard;
	opt.filter_milestone.clear();
	if (boot_trace_parser_run(bad, discard, opt, err))
		return 1;
	return 0;
}
