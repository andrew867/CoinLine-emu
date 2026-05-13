// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_mach_pio.h"

#include <iostream>

int main()
{
	// Firmware keypad loop writes 0x06 — shadow low nibble; cash bits 2-3 must read as OK (clear).
	std::uint8_t const r = millennium_mach_pio_combine_port_h_read(0x06U, 0x00U);
	if (r != 0x02U) {
		std::cerr << "expected 0x02 got 0x" << std::hex << int(r) << '\n';
		return 1;
	}
	// UPPER_RAM_ENABLE 0x07 — after cash-bit masking low status bits 2-3 clear -> 0x03
	if (millennium_mach_pio_combine_port_h_read(0x07U, 0x00U) != 0x03U)
		return 1;

	constexpr std::size_t cap = 0x80000U;
	// Port H 0x07: bank 0 from high nibble 0; upper window enabled (low three bits set).
	auto d = millennium_mach_decode_phys_ram(0x07U, 0xc0000U, cap);
	if (d.route != millennium_mach_phys_ram_route::sram_chip || d.chip_byte_index != 0U)
		return 2;
	d = millennium_mach_decode_phys_ram(0x07U, 0xe0000U, cap);
	if (d.route != millennium_mach_phys_ram_route::sram_chip || d.chip_byte_index != 0x20000U)
		return 3;
	// Upper window disabled: 0x06 clears one enable bit — high region tracks flash path.
	d = millennium_mach_decode_phys_ram(0x06U, 0xe0000U, cap);
	if (d.route != millennium_mach_phys_ram_route::upper_flash)
		return 4;
	d = millennium_mach_decode_phys_ram(0x06U, 0xc8000U, cap);
	if (d.route != millennium_mach_phys_ram_route::sram_chip || d.chip_byte_index != 0x8000U)
		return 5;

	return 0;
}
