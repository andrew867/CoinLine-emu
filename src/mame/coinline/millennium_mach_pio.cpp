// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_mach_pio.h"
#include "millennium_cashbox_hw.h"

namespace {

constexpr std::uint32_t k_window_span = 0x20000U;

} // namespace

std::uint8_t millennium_mach_pio_combine_port_h_read(std::uint8_t shadow, std::uint8_t smartcard_lines_low2)
{
	// Clear vault/cover bits 2–3 on read (nominal “OK” for polls). Low **two** bits overlap the RAM latch;
	// OR in smart-card presence lines — matches keypad-loop shadows (\c 0x06 / \c 0x07) when SC lines are idle.
	std::uint8_t const cleared = static_cast<std::uint8_t>(shadow & ~millennium_cashbox_hw::k_mach_port_h_vault_status_mask);
	return static_cast<std::uint8_t>(cleared | (smartcard_lines_low2 & 0x03U));
}

millennium_mach_phys_ram_decode millennium_mach_decode_phys_ram(std::uint8_t mach_port_h_shadow,
	std::uint32_t phys_addr_20, std::size_t sram_capacity_bytes)
{
	millennium_mach_phys_ram_decode out{};
	std::uint32_t const phys = phys_addr_20 & 0xfffffU;
	if (phys < 0xc0000U) {
		out.route = millennium_mach_phys_ram_route::below_sram_windows;
		return out;
	}
	unsigned const bank = (mach_port_h_shadow >> 4) & 3U;
	if (phys <= 0xdffffU) {
		std::size_t const idx = std::size_t(bank) * k_window_span + (phys - 0xc0000U);
		if (idx < sram_capacity_bytes) {
			out.route = millennium_mach_phys_ram_route::sram_chip;
			out.chip_byte_index = idx;
			return out;
		}
		out.route = millennium_mach_phys_ram_route::unmapped_ff;
		return out;
	}
	if ((mach_port_h_shadow & 0x07U) == 0x07U) {
		std::uint64_t const idx64 = std::uint64_t(bank) * k_window_span + k_window_span + (phys - 0xe0000U);
		if (idx64 < sram_capacity_bytes) {
			out.route = millennium_mach_phys_ram_route::sram_chip;
			out.chip_byte_index = static_cast<std::size_t>(idx64);
			return out;
		}
	}
	out.route = millennium_mach_phys_ram_route::upper_flash;
	return out;
}
