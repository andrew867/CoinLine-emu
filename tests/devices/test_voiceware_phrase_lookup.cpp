// SPDX-License-Identifier: GPL-2.0-or-later
// T0.3 — phrase directory → uPD sample index (synthetic NEC-style header).

#include "millennium_voiceware_phrase_lookup.h"

#include <array>

int main()
{
	// 128 KiB bank 0: one segment, one message. Directory word offset 4 → message starts at byte 8.
	std::array<std::uint8_t, 0x20000> bank0{};
	bank0[0] = 0x00; // seg_last → 1 message
	bank0[1] = 0x5a;
	bank0[2] = 0xa5;
	bank0[3] = 0x69;
	bank0[4] = 0x55;
	bank0[5] = 0x00; // dir hi
	bank0[6] = 0x04; // dir lo → word 4 → byte offset 8
	bank0[8] = 0x00; // message_mode
	bank0[9] = 0x00; // end opcode

	coinline_voiceware_phrase_lookup_result out{};
	if (!coinline_voiceware_lookup_phrase_for_upd7759(bank0.data(), std::uint32_t(bank0.size()), 0x00U, 0x00U, out))
		return 1;
	if (!out.ok || out.upd_sample_index != 0U || out.segment_base_abs != 0U || out.msg_count != 1U)
		return 2;

	coinline_voiceware_phrase_lookup_result bad{};
	if (coinline_voiceware_lookup_phrase_for_upd7759(bank0.data(), std::uint32_t(bank0.size()), 0x01U, 0x00U, bad))
		return 3;

	if (coinline_voiceware_lookup_phrase_for_upd7759(bank0.data(), std::uint32_t(bank0.size()), 0x00U, 0x01U, bad))
		return 4;

	return 0;
}
