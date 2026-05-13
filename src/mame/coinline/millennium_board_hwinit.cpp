// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_board_hwinit.h"

namespace {

constexpr std::uint8_t k_vfd_csb = 0x40U;
constexpr std::uint8_t k_upper_ram_enable = 0x07U;

} // namespace

void millennium_hwinit_apply_pio_port_initialize(bool new_hardware_revision_1,
	std::array<std::uint8_t, 4> &pio_8255_shadow,
	std::uint8_t &pio_port_g,
	std::array<std::uint8_t, 4> &mach_pio_shadow)
{
	/*
	 * Ordering: EPM power sequencing on ports G/C, then MACH ports E/F cleared, port H upper-RAM enable.
	 * Shadow indices match the machine’s \c 0xC0–\c 0xC3 map (H, D, E, F).
	 */
	pio_8255_shadow[0] = new_hardware_revision_1 ? 0xBFU : 0xFFU;
	pio_8255_shadow[1] = static_cast<std::uint8_t>(~k_vfd_csb);
	pio_8255_shadow[2] = 0x00U;

	pio_port_g = 0x00U;

	mach_pio_shadow[0] = k_upper_ram_enable;
	mach_pio_shadow[1] = 0x00U;
	mach_pio_shadow[2] = 0x00U;
	mach_pio_shadow[3] = 0x00U;
}

void millennium_hwinit_apply_coin_validator_tl16c550(std::array<std::uint8_t, 8> &ext_uart_shadow)
{
	ext_uart_shadow.fill(0);
	ext_uart_shadow[1] = 0x01U;
	ext_uart_shadow[2] = 0x01U;
	ext_uart_shadow[3] = 0x03U;
	ext_uart_shadow[4] = 0x00U;
	ext_uart_shadow[5] = 0x60U;
	ext_uart_shadow[6] = 0xB0U;
}
