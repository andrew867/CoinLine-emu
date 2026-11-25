// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>

/// Result of mapping a host phrase byte to uPD7759 **master-mode** sample index for the **current** ROM bank.
struct coinline_voiceware_phrase_lookup_result {
	bool ok = false;
	/// Value for `upd7759_device::port_w` when `ok`.
	std::uint8_t upd_sample_index = 0;
	unsigned resolved_segment_index = 0;
	std::uint32_t segment_base_abs = 0;
	unsigned msg_count = 0;
	std::uint8_t phrase_msg_index = 0;
};

/// Resolves a sample against the directory in the selected 128 KiB ROM window.
/// The synthesizer sees only that window (bit 3 selects the chip, bits 2:0 select the window).
bool coinline_voiceware_lookup_phrase_for_upd7759(std::uint8_t const *rom, std::uint32_t rom_bytes,
	std::uint8_t bank_latch, std::uint8_t phrase, coinline_voiceware_phrase_lookup_result &out);
