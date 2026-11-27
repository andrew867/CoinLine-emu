// SPDX-License-Identifier: GPL-2.0-or-later
// T0.3 — phrase directory → uPD sample index (synthetic NEC-style header).

#include "millennium_voiceware_phrase_lookup.h"
#include "sound/upd7759_waveform.h"

#include <algorithm>
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

	std::array<std::uint8_t, 0x40000> rom{};
	std::copy(bank0.begin(), bank0.end(), rom.begin());
	std::copy(bank0.begin(), bank0.end(), rom.begin() + bank0.size());
	if (!coinline_voiceware_lookup_phrase_for_upd7759(rom.data(), std::uint32_t(rom.size()), 0x01U, 0x00U, out))
		return 5;
	if (!out.ok || out.upd_sample_index != 0U || out.segment_base_abs != 0x20000U)
		return 6;

	// Invented waveform fixture, not a device ROM sample. Two periods of
	// eight points, divider 81, followed by an explicit pause and terminator.
	bank0[8] = 0x40;
	std::array<std::uint8_t, 14> const wave = {0xc4, 0x53, 0x42, 1,
		0, 192, 255, 192, 128, 64, 0, 64, 0x2f, 0};
	std::copy(wave.begin(), wave.end(), bank0.begin() + 9);
	if (!coinline_voiceware_lookup_phrase_for_upd7759(bank0.data(), std::uint32_t(bank0.size()), 0, 0, out))
		return 7;
	if (out.message_mode != 0x40 || out.waveform_clocks != 16 * 81 + 48 * 1024)
		return 8;
	auto read = [&bank0](std::uint32_t address) { return bank0[address]; };
	std::uint32_t pos = 9;
	std::uint16_t divider = 0;
	std::uint8_t gain = 0;
	upd7759_waveform::element element;
	using result = upd7759_waveform::result;
	if (upd7759_waveform::next_element(read, pos, 23, divider, gain, element) != result::block ||
		element.length != 8 || element.points != 16 || element.divider != 81 || element.gain != 4)
		return 9;
	if (upd7759_waveform::dac_sample(0, 0, 4) != 0 ||
		upd7759_waveform::dac_sample(0, 5, 4) >= 0 || upd7759_waveform::dac_sample(255, 2, 4) <= 0 ||
		upd7759_waveform::dac_sample(255, 2, 7) <= upd7759_waveform::dac_sample(255, 2, 4))
		return 10;
	if (upd7759_waveform::next_element(read, pos, 23, divider, gain, element) != result::block ||
		element.length != 0 || element.clocks != 48 * 1024 ||
		upd7759_waveform::next_element(read, pos, 23, divider, gain, element) != result::end)
		return 11;
	// A truncated waveform or unexpected opcode cannot spill into another record.
	std::uint32_t clocks = 0;
	if (upd7759_waveform::validate(read, 9, 14, clocks) || upd7759_waveform::validate(read, 9, 22, clocks))
		return 12;
	bank0[9] = 0x80;
	if (coinline_voiceware_lookup_phrase_for_upd7759(bank0.data(), std::uint32_t(bank0.size()), 0, 0, out))
		return 13;
	bank0[9] = 0xc4;
	bank0[0] = 1; // second directory entry at byte 16 limits the first record
	bank0[7] = 0;
	bank0[8] = 8;
	// Realign first message after the expanded directory, beginning at byte 10.
	bank0[6] = 5;
	bank0[10] = 0x40;
	std::copy(wave.begin(), wave.end(), bank0.begin() + 11);
	if (coinline_voiceware_lookup_phrase_for_upd7759(bank0.data(), std::uint32_t(bank0.size()), 0, 0, out))
		return 14;

	return 0;
}
