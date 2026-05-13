// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_audio_trace.h"

#include <iostream>
#include <string>

int main()
{
	coinline_voiceware_phrase_trace tr{};
	tr.emulated_time_ns = 1;
	tr.cycle = 2;
	tr.pc = 0x5b29;
	tr.sp = 0x7f00;
	tr.event_type = "voice_segment_start";
	tr.port = 0x61;
	tr.rw = 'w';
	tr.value = 0xb3;
	tr.raw_sample_index = 0xb3;
	tr.bank_latch = 3;
	tr.chip_bank = 3;
	tr.active_rom_id = "U16";
	tr.active_blob = "unmapped_table";
	tr.mapping_confidence = "unknown";
	tr.upd7759_idle_before = true;
	tr.upd7759_idle_after = false;
	tr.playback_started = true;
	tr.notes = "test";
	std::string const j = millennium_audio_trace_voiceware_phrase_detail_json(tr);
	if (j.find("coinline.audio_trace/v1") == std::string::npos || j.find("0xB3") == std::string::npos) {
		std::cerr << "unexpected phrase detail json\n";
		return 1;
	}
	return 0;
}
