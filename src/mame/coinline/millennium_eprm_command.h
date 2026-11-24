// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>

/// Eleven-bit Microwire command body shifted after START on the later motherboard.
/// Eight bits are the opcode byte merged with the word index, then two more address bits.
/// One trailing clock with DI low follows, so the stored body is that 10-bit field shifted left by one.
///
/// \p opcode_byte READ is 0x80, WRITE is 0x40, write-enable is 0x30.
/// \p byte_offset_c Byte offset of the 16-bit word (even addresses).
///
/// The serial model shifts START into the SR before the opcode bits. The on-wire body for that
/// decoder is \c (0x400U | ((millennium_eprm_command_body11(op,c) >> 1) & 0x3FFU)).
inline constexpr std::uint16_t millennium_eprm_command_body11(std::uint8_t opcode_byte,
	std::uint8_t byte_offset_c)
{
	std::uint8_t c = byte_offset_c;
	std::uint8_t a = 0;
	bool cy = false;

	cy = (c & 1U) != 0;
	c = static_cast<std::uint8_t>(c >> 1);
	cy = (c & 1U) != 0;
	c = static_cast<std::uint8_t>(c >> 1);
	{
		std::uint8_t const old_a = a;
		a = static_cast<std::uint8_t>((a >> 1U) | (cy ? 0x80U : 0U));
		cy = (old_a & 1U) != 0;
	}
	cy = (c & 1U) != 0;
	c = static_cast<std::uint8_t>(c >> 1);
	{
		std::uint8_t const old_a = a;
		a = static_cast<std::uint8_t>((a >> 1U) | (cy ? 0x80U : 0U));
		cy = (old_a & 1U) != 0;
	}
	std::uint8_t const l = a;
	std::uint8_t h = static_cast<std::uint8_t>(opcode_byte | c);

	std::uint16_t body10 = 0;
	for (int i = 0; i < 8; ++i) {
		bool const bit = (h & 0x80U) != 0;
		body10 = static_cast<std::uint16_t>((body10 << 1U) | (bit ? 1U : 0U));
		h = static_cast<std::uint8_t>((h << 1U) & 0xffU);
	}
	std::uint8_t lv = l;
	for (int i = 0; i < 2; ++i) {
		bool const bit = (lv & 0x80U) != 0;
		body10 = static_cast<std::uint16_t>((body10 << 1U) | (bit ? 1U : 0U));
		lv = static_cast<std::uint8_t>((lv << 1U) & 0xffU);
	}
	return static_cast<std::uint16_t>((body10 << 1U) & 0x7ffU);
}
