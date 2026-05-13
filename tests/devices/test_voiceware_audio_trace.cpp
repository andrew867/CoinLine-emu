// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_audio_trace.h"

#include <iostream>
#include <string>

int main()
{
	std::string const j = millennium_audio_trace_voiceware_json(1000ULL, 2000ULL, 0x5b29U, "voice_segment_start", 0x61U, 'w',
		0xb3U, 0U, 0xb3U, "compatibility_validation_required");
	if (j.find("coinline.audio_trace/v1") == std::string::npos || j.find("voice_segment_start") == std::string::npos) {
		std::cerr << "audio trace json shape unexpected\n";
		return 1;
	}
	return 0;
}
