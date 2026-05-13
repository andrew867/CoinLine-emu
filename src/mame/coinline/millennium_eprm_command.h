// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>

/// Behavioural model of the 11-bit Microwire command body emitted by the terminal when
/// driving the on-board 93C66 EEPROM. After the START bit the host shifts 10 DI bits — eight
/// opcode/address-MSB bits derived from the upper byte (opcode high nibble OR'd with the high
/// bits of the byte offset), then the two MSBs of the lower byte (remaining address bits) — and
/// the serial decoder picks up an 11th bit as zero on the next SK rise, so
/// \c body11 = (\c body10 << 1) & 0x7FF.
///
/// \p iodefs_opcode_byte The encoded EEPROM opcode byte: READ = 0x80, WRITE = 0x40,
/// erase/write-enable = 0x30, etc. (Microwire opcode high nibble in the upper four bits.)
/// \p byte_offset_c   Byte offset within the persistent-storage structure; supplies the
/// low-order address bits before the rotate-through-carry sequence.
///
/// The MAME Microwire device shifts START into its internal SR before the opcode bits,
/// so the on-wire body11 it consumes is
/// \c (0x400U | ((millennium_eprm_command_body11(op,c) >> 1) & 0x3FFU)).
inline constexpr std::uint16_t millennium_eprm_command_body11(std::uint8_t iodefs_opcode_byte,
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
	std::uint8_t h = static_cast<std::uint8_t>(iodefs_opcode_byte | c);

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
