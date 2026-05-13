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

/// Walks the same NEC-style directory layout as the legacy software decoder (**reference** segment walk on one voice ROM).
/// Succeeds only when the resolved segment base matches the **physical** 128 KiB window selected by `bank_latch` (bits 3: chip, 2:0: window).
bool coinline_voiceware_lookup_phrase_for_upd7759(std::uint8_t const *rom, std::uint32_t rom_bytes,
	std::uint8_t bank_latch, std::uint8_t phrase, coinline_voiceware_phrase_lookup_result &out);
