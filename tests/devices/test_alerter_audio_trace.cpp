// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_audio_trace.h"

#include <iostream>
#include <string>

int main()
{
	std::string const r = millennium_audio_trace_alerter_ready_json(1ULL, 2ULL, 0x4000U);
	std::string const g =
		millennium_audio_trace_alerter_gpio_json(3ULL, 4ULL, 0x4000U, 0x0058U, 0x0EU);
	std::string const ts =
		millennium_audio_trace_alerter_tone_start_json(5ULL, 6ULL, 0x0000U, "service_beep", 0U, 0U);
	std::string const te = millennium_audio_trace_alerter_tone_end_json(7ULL, 8ULL, 0x0000U, "service_beep", 65U, 1U);
	if (r.find("alerter_ready") == std::string::npos || r.find("\"device\":\"audio\"") == std::string::npos) {
		std::cerr << "alerter_ready json unexpected\n";
		return 1;
	}
	if (g.find("alerter_gpio_write") == std::string::npos || g.find("\"0x0058\"") == std::string::npos) {
		std::cerr << "alerter_gpio json unexpected\n";
		return 1;
	}
	if (ts.find("alerter_tone_start") == std::string::npos || te.find("alerter_tone_end") == std::string::npos) {
		std::cerr << "alerter tone json unexpected\n";
		return 1;
	}
	return 0;
}
