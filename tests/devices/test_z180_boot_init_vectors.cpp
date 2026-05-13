// SPDX-License-Identifier: GPL-2.0-or-later
// Vector checks aligned to terminal_15_z180_boot_init_emulator_spec.yaml.

#include "millennium_z180_boot_init_model.h"

#include <cstdint>
#include <iostream>

int main()
{
	using namespace coinline::z180;

	boot_init_model m;
	boot_init_config cfg{};
	cfg.banked_mode = true;
	cfg.stack_addr = 0x0450U;
	cfg.x_mem_top_addr = 0x80FEU;
	m.configure(cfg);
	m.reset();
	m.set_bss_size(64U);

	m.run_cstart();
	if (!m.bank_init_called() || !m.jumped_xreset1() || m.sp() != 0x0450U || m.bank_init_call_count() != 1U) {
		std::cerr << "banked boot nominal vector failed\n";
		return 1;
	}
	m.run_pre_reset_handoff();
	if (m.bank_init_call_count() != 2U) {
		std::cerr << "second bank-init handoff missing\n";
		return 5;
	}

	m.run_bss_clear_and_malloc_seed();
	for (std::size_t i = 0; i < m.bss().size(); ++i) {
		std::uint8_t const b = m.bss()[i];
		if (i == 0U && b != 0xA5U) {
			std::cerr << "battery-backed sentinel byte should be preserved\n";
			return 6;
		}
		if (i != 0U && b != 0x00U) {
			std::cerr << "BSS clear vector failed\n";
			return 2;
		}
	}
	if (m.freep() != 0x00U || m.heap_base() != 0x00U) {
		std::cerr << "malloc seed vector failed\n";
		return 3;
	}

	m.set_mem_top(0xDEU);
	if (!m.detect_mem_top_corruption(0xDEU) || m.detect_mem_top_corruption(0xADU)) {
		std::cerr << "memory top sentinel vector failed\n";
		return 4;
	}

	{
		boot_init_model n;
		boot_init_config ncfg{};
		ncfg.banked_mode = false;
		ncfg.battery_backed = false;
		ncfg.stack_addr = 0x0550U;
		ncfg.cbr_value = 0x12U;
		ncfg.cbar_value = 0x34U;
		n.configure(ncfg);
		n.reset();
		n.set_bss_size(8U);
		n.run_cstart();
		n.run_bss_clear_and_malloc_seed();
		if (!n.non_banked_mmu_configured() || n.cbr() != 0x12U || n.cbar() != 0x34U || n.bank_init_call_count() != 0U) {
			std::cerr << "non-banked MMU path failed\n";
			return 7;
		}
		for (std::uint8_t b : n.bss()) {
			if (b != 0x00U) {
				std::cerr << "non-battery BSS clear failed\n";
				return 8;
			}
		}
	}

	std::cout << "z180_boot_init_vectors ok\n";
	return 0;
}

