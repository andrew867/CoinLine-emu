// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>

/// CPU read data presented on the Z180 voiceware phrase / status port (`0x61`).
/// Defaults match long-standing firmware bring-up (`0xFF` idle / `0x7F` busy). Override
/// per lab capture via `COINLINE_VOICEWARE_0x61_*` until a formal datasheet lands in spec.
struct coinline_voiceware_phrase_port_levels {
	std::uint8_t idle_read = 0xffU;
	std::uint8_t busy_read = 0x7fU;
	/// Returned while voice reset is asserted (host `0x40` path); defaults to `busy_read`.
	std::uint8_t fault_read = 0x7fU;
};

/// Parse `0xAB` / `AB` / `ab` (1–2 hex digits). Returns false if invalid / empty.
bool coinline_voiceware_parse_hex_u8(char const *s, std::uint8_t &out);

/// Start from `levels` (typically legacy defaults) and overlay any non-null env strings.
void coinline_voiceware_phrase_port_levels_apply_hex_env(coinline_voiceware_phrase_port_levels &levels,
	char const *env_idle, char const *env_busy, char const *env_fault);
