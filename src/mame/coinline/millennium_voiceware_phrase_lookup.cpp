// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_voiceware_phrase_lookup.h"

bool coinline_voiceware_lookup_phrase_for_upd7759(std::uint8_t const *rom, std::uint32_t rom_bytes,
	std::uint8_t bank_latch, std::uint8_t phrase, coinline_voiceware_phrase_lookup_result &out)
{
	out = {};
	std::uint32_t constexpr window_bytes = 0x20000U;
	std::uint32_t const segment_base = std::uint32_t(bank_latch & 0x0fU) * window_bytes;
	if (!rom || segment_base + 7U > rom_bytes)
		return false;

	auto const *const bank = rom + segment_base;
	bool const valid_header = bank[1] == 0x5aU && bank[2] == 0xa5U && bank[3] == 0x69U && bank[4] == 0x55U;
	std::uint8_t const last_sample = bank[0];
	if (!valid_header || last_sample > 0x7fU || phrase > last_sample)
		return false;

	std::uint32_t const directory_end = 5U + 2U * (std::uint32_t(last_sample) + 1U);
	if (segment_base + directory_end > rom_bytes)
		return false;
	std::uint32_t const dir_offset = 5U + 2U * std::uint32_t(phrase);
	std::uint8_t const hi = bank[dir_offset];
	std::uint8_t const lo = bank[dir_offset + 1U];
	std::uint32_t const msg_offset = std::uint32_t((unsigned(hi) << 8) | unsigned(lo)) * 2U;
	if (msg_offset < directory_end || segment_base + msg_offset + 1U >= rom_bytes)
		return false;

	// Other container modes do not contain a standalone uPD sample stream.
	if (bank[msg_offset] != 0x00U)
		return false;

	out.ok = true;
	out.upd_sample_index = phrase;
	out.resolved_segment_index = bank_latch & 0x07U;
	out.segment_base_abs = segment_base;
	out.msg_count = unsigned(last_sample) + 1U;
	out.phrase_msg_index = phrase;
	return true;
}
