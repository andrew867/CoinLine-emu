// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>

/// Bit semantics for the MACH PIO block and port \c 0x63 glue used by the smart-card / async ISO7816 path.
/// EEPROM-module (EPM) clock enables live on port \c G; device addressing on port \c D; SAM clock gate on port \c E.
/// Host firmware routines that only deliver RTOS signals or tear down timers are not emulated — only port shadows matter here.

namespace millennium_mach_async {

inline constexpr std::uint8_t k_h_async_gate_bit = 0x08U; ///< When **clear**, async mux path is enabled toward the UART/ASCI front-end.
inline constexpr std::uint8_t k_d_device_addr_mask = 0x03U;

enum class device_addr : std::uint8_t {
	coin_path = 0,
	mag_reader = 1,
	sam_socket = 2,
	electronic_lock = 3,
};

inline constexpr std::uint8_t k_g_epm_clock_mask = 0x38U; ///< Bits 3–5: one-hot style EPM clock enables.
inline constexpr std::uint8_t k_g_clock_epm0 = 0x08U;
inline constexpr std::uint8_t k_g_clock_epm1 = 0x10U;
inline constexpr std::uint8_t k_g_clock_epm2 = 0x20U;

inline constexpr std::uint8_t k_e_sam_clk_cntl_mask = 0x02U; ///< Bit 1: SAM serial clock / bit-bang enable (shared E latch).

constexpr bool h_async_path_enabled(std::uint8_t mach_port_h_latch) noexcept
{
	return (mach_port_h_latch & k_h_async_gate_bit) == 0U;
}

constexpr std::uint8_t d_device_addr(std::uint8_t mach_port_d_latch) noexcept
{
	return mach_port_d_latch & k_d_device_addr_mask;
}

constexpr std::uint8_t g_epm_clock_select(std::uint8_t port_g_latch) noexcept
{
	return port_g_latch & k_g_epm_clock_mask;
}

} // namespace millennium_mach_async
