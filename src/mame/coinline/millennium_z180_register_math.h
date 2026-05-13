// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>

// Bit-exact mirrors of z180_internal_port_read() masking for key registers (see z180.cpp).

inline std::uint8_t millennium_z180_itc_read_byte(std::uint8_t raw_itc)
{
	constexpr std::uint8_t Z180_ITC_MASK = 0xc7;
	return std::uint8_t(raw_itc | ~Z180_ITC_MASK);
}

inline std::uint8_t millennium_z180_rcr_read_byte(std::uint8_t raw_rcr)
{
	constexpr std::uint8_t Z180_RCR_MASK = 0xc3;
	return std::uint8_t(raw_rcr | ~Z180_RCR_MASK);
}

inline std::uint8_t millennium_z180_iocr_read_byte(std::uint8_t raw_iocr, bool extended_io)
{
	return std::uint8_t(raw_iocr | ~(extended_io ? 0xa0 : 0xe0));
}

inline std::uint8_t millennium_z180_il_read_byte(std::uint8_t raw_il)
{
	return std::uint8_t(raw_il & 0xe0);
}

/// See z180.cpp \c z180_internal_port_read case 0x3e (M1TE forced on read path).
inline std::uint8_t millennium_z180_omcr_read_byte(std::uint8_t raw_omcr)
{
	constexpr std::uint8_t Z180_OMCR_M1TE = 0x40;
	constexpr std::uint8_t Z180_OMCR_MASK = 0xe0;
	return std::uint8_t(raw_omcr | Z180_OMCR_M1TE | ~Z180_OMCR_MASK);
}
