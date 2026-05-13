// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_z180_boot_init_model.h"

namespace coinline::z180 {

void boot_init_model::reset() noexcept
{
	m_sp = 0U;
	m_freep = 0xFFU;
	m_heap_base = 0xFFU;
	m_cbr = 0x00U;
	m_cbar = 0x00U;
	m_mem_top = 0x00U;
	m_bank_init_called = false;
	m_bank_init_call_count = 0U;
	m_non_banked_mmu_configured = false;
	m_jumped_xreset1 = false;
	m_bss.clear();
}

void boot_init_model::set_bss_size(std::size_t bytes)
{
	m_bss.assign(bytes, 0xA5U);
}

void boot_init_model::run_cstart() noexcept
{
	m_sp = m_cfg.stack_addr;
	if (m_cfg.banked_mode) {
		m_bank_init_called = true;
		m_bank_init_call_count++;
	} else {
		m_non_banked_mmu_configured = true;
		m_cbr = m_cfg.cbr_value;
		m_cbar = m_cfg.cbar_value;
	}
	m_jumped_xreset1 = true;
}

void boot_init_model::run_bss_clear_and_malloc_seed() noexcept
{
	for (std::size_t i = 0; i < m_bss.size(); ++i) {
		// Battery-backed mode preserves the first byte as retained state sentinel.
		if (m_cfg.battery_backed && i == 0U)
			continue;
		m_bss[i] = 0x00U;
	}
	m_freep = 0x00U;
	m_heap_base = 0x00U;
}

void boot_init_model::run_pre_reset_handoff() noexcept
{
	if (m_cfg.banked_mode) {
		m_bank_init_called = true;
		m_bank_init_call_count++;
	}
}

bool boot_init_model::detect_mem_top_corruption(std::uint8_t corrupt_marker) const noexcept
{
	return m_mem_top == corrupt_marker;
}

} // namespace coinline::z180

