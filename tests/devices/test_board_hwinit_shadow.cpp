// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_board_hwinit.h"

#include <array>
#include <cstdint>
#include <iostream>

int main()
{
	std::array<std::uint8_t, 4> pio{};
	std::array<std::uint8_t, 4> mach{};
	pio.fill(0xff);
	std::uint8_t g = 0xff;
	millennium_hwinit_apply_pio_port_initialize(true, pio, g, mach);
	if (pio[0] != 0xBFU || pio[1] != 0xBFU || pio[2] != 0U || g != 0U)
		return 1;
	if (mach[0] != 0x07U || mach[1] != 0U || mach[2] != 0U || mach[3] != 0U)
		return 2;

	pio.fill(0xff);
	g = 0xff;
	millennium_hwinit_apply_pio_port_initialize(false, pio, g, mach);
	if (pio[0] != 0xFFU)
		return 3;

	std::array<std::uint8_t, 8> uart{};
	millennium_hwinit_apply_coin_validator_tl16c550(uart);
	if (uart[1] != 0x01U || uart[3] != 0x03U || uart[4] != 0U || uart[5] != 0x60U || uart[6] != 0xB0U)
		return 4;

	std::cout << "board_hwinit_shadow ok\n";
	return 0;
}
