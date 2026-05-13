// SPDX-License-Identifier: GPL-2.0-or-later

#include "io_trace_analyzer.h"

#include <fstream>
#include <sstream>
#include <string>

#ifndef COINLINE_EMU_SOURCE_DIR
#error COINLINE_EMU_SOURCE_DIR
#endif

int main()
{
	std::string const trace_path = std::string(COINLINE_EMU_SOURCE_DIR) + "/tools/io-trace-analyzer/tests/fixtures/sample-io-trace.jsonl";
	std::string const map_path = std::string(COINLINE_EMU_SOURCE_DIR) + "/fixtures/board/io-port-map.json";

	std::ifstream in(trace_path);
	if (!in)
		return 1;
	std::ostringstream out;
	std::string err;
	IoTraceAnalyzerOptions opt{};
	opt.filter_port = "0x60";
	if (!io_trace_analyzer_run(in, out, opt, err))
		return 1;
	std::string const exp =
		"0x60\t10\tw\t0x00\n"
		"0x60\t11\tw\t0x01\n";
	if (out.str() != exp)
		return 1;

	std::ifstream in2(trace_path);
	std::ostringstream out2;
	opt = IoTraceAnalyzerOptions{};
	opt.histogram = true;
	opt.format = IoTraceOutputFormat::table;
	if (!io_trace_analyzer_run(in2, out2, opt, err))
		return 1;
	std::string const exp_hist =
		"port\tcount\n"
		"0x60\t2\n"
		"0xA0\t2\n";
	if (out2.str() != exp_hist)
		return 1;

	std::ifstream in3(trace_path);
	std::ostringstream out3;
	opt = IoTraceAnalyzerOptions{};
	opt.filter_device = "vfd";
	opt.port_map_path = map_path;
	opt.histogram = true;
	if (!io_trace_analyzer_run(in3, out3, opt, err))
		return 1;
	std::string const exp_vfd =
		"port\tcount\n"
		"0x60\t2\n";
	if (out3.str() != exp_vfd)
		return 1;

	std::istringstream bad("{bad\n");
	std::ostringstream discard;
	opt = IoTraceAnalyzerOptions{};
	if (io_trace_analyzer_run(bad, discard, opt, err))
		return 1;
	return 0;
}
