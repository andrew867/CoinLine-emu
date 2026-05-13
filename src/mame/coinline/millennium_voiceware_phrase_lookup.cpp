// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_voiceware_phrase_lookup.h"

bool coinline_voiceware_lookup_phrase_for_upd7759(std::uint8_t const *rom, std::uint32_t rom_bytes,
	std::uint8_t bank_latch, std::uint8_t phrase, coinline_voiceware_phrase_lookup_result &out)
{
	out = {};
	if (!rom || rom_bytes < 6U)
		return false;

	std::uint32_t const chip_base = ((bank_latch & 0x08U) != 0U) ? 0x100000U : 0U;
	std::uint32_t const expected_segment_base = chip_base + std::uint32_t(bank_latch & 0x07U) * 0x20000U;

	std::uint32_t constexpr window_bytes = 0x20000U;
	unsigned remaining = unsigned(phrase);
	bool found = false;
	unsigned resolved_segment_index = 0;
	unsigned msg_index = 0;
	unsigned msg_count = 0;
	std::uint32_t stream_base = 0;

	for (unsigned seg = 0; seg < 8U; ++seg) {
		stream_base = seg * 0x20000U;
		std::uint32_t const abs0 = chip_base + stream_base;
		if (abs0 + 6U > rom_bytes)
			break;

		std::uint8_t const seg_last = rom[abs0];
		std::uint8_t const m0 = rom[abs0 + 1U];
		std::uint8_t const m1 = rom[abs0 + 2U];
		std::uint8_t const m2 = rom[abs0 + 3U];
		std::uint8_t const m3 = rom[abs0 + 4U];
		bool const seg_magic_ok = (m0 == 0x5aU && m1 == 0xa5U && m2 == 0x69U && m3 == 0x55U);
		unsigned const seg_count = unsigned(seg_last) + 1U;
		if (!seg_magic_ok || seg_last > 0x7fU || (5U + 2U * seg_count) >= window_bytes)
			continue;
		if (remaining < seg_count) {
			resolved_segment_index = seg;
			msg_index = remaining;
			msg_count = seg_count;
			found = true;
			break;
		}
		remaining -= seg_count;
	}
	if (!found)
		return false;

	std::uint32_t const segment_base_abs = chip_base + stream_base;
	if (segment_base_abs != expected_segment_base)
		return false;

	std::uint32_t const abs0 = chip_base + stream_base;
	std::uint8_t const seg_last = rom[abs0];
	if (msg_index >= unsigned(seg_last) + 1U)
		return false;

	std::uint32_t const dir_offset = 5U + 2U * std::uint32_t(msg_index);
	if (dir_offset + 2U > window_bytes || abs0 + dir_offset + 2U > rom_bytes)
		return false;
	std::uint8_t const hi = rom[abs0 + dir_offset];
	std::uint8_t const lo = rom[abs0 + dir_offset + 1U];
	std::uint32_t const msg_offset = std::uint32_t((unsigned(hi) << 8) | unsigned(lo)) * 2U;
	if (abs0 + msg_offset + 1U >= rom_bytes)
		return false;
	std::uint8_t const message_mode = rom[abs0 + msg_offset];
	if (message_mode != 0x00U)
		return false;

	out.ok = true;
	out.upd_sample_index = static_cast<std::uint8_t>(msg_index);
	out.resolved_segment_index = resolved_segment_index;
	out.segment_base_abs = segment_base_abs;
	out.msg_count = msg_count;
	out.phrase_msg_index = static_cast<std::uint8_t>(msg_index);
	return true;
}
