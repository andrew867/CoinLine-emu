// SPDX-License-Identifier: GPL-2.0-or-later

#include "evidence_export_client.h"

#include <iostream>
#include <string>

namespace {

void print_help_short()
{
	std::cout << "evidence-bundle-export — pull a framed evidence payload from coinline-mame (TCP), or pack a run directory\n"
		  << "\n"
		  << "Wire format: 4-byte big-endian length, then UTF-8 JSON (see README). Large blobs use *_b64 fields.\n"
		  << "\n"
		  << "Usage:\n"
		  << "  evidence-bundle-export --emulator <host:port> --output <dir> [--scenario <id>]\n"
		  << "  evidence-bundle-export --from-run-dir <run_dir> --output <dir> [--scenario <id>]\n"
		  << "\n"
		  << "Examples:\n"
		  << "  evidence-bundle-export --emulator 127.0.0.1:9123 --output out/bundle\n"
		  << "  evidence-bundle-export --from-run-dir ./build/runs/latest-audio --output out/bundle-audio\n"
		  << "\n"
		  << "Use --help-all for every flag.\n";
}

void print_help_all()
{
	print_help_short();
	std::cout << "\nFlags:\n"
		  << "  --emulator <host:port>   IPv4 TCP endpoint (emulator listens; tool connects)\n"
		  << "  --output <dir>           Write evidence bundle directory (created/replaced)\n"
		  << "  --from-run-dir <dir>    Import traces from a coinline-mame run folder (no TCP)\n"
		  << "  --scenario <id>          Override scenario_id after parse (optional)\n"
		  << "  --help, -h               Short help\n"
		  << "  --help-all               This text\n";
}

} // namespace

int main(int argc, char **argv)
{
	if (argc <= 1) {
		print_help_short();
		std::cerr << "\nerror: no arguments (use --help)\n";
		return 1;
	}
	std::string emulator, output, scenario, from_run;
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
		if (a == "--emulator" && i + 1 < argc)
			emulator = argv[++i];
		else if (a == "--output" && i + 1 < argc)
			output = argv[++i];
		else if (a == "--from-run-dir" && i + 1 < argc)
			from_run = argv[++i];
		else if (a == "--scenario" && i + 1 < argc)
			scenario = argv[++i];
		else {
			std::cerr << "error: unknown argument: " << a << "\n";
			std::cerr << "Try evidence-bundle-export --help\n";
			return 1;
		}
	}
	if (output.empty()) {
		std::cerr << "error: --output is required\n";
		return 1;
	}
	if ((!emulator.empty()) == (!from_run.empty())) {
		std::cerr << "error: specify exactly one of --emulator <host:port> or --from-run-dir <dir>\n";
		return 1;
	}

	std::string err;
	if (!from_run.empty()) {
		if (!evidence_bundle_export_from_run_directory(from_run, output, scenario, err)) {
			std::cerr << "error: " << err << '\n';
			return 1;
		}
		return 0;
	}
	if (!evidence_bundle_export_run(emulator, output, scenario, err)) {
		std::cerr << "error: " << err << '\n';
		return 1;
	}
	return 0;
}
