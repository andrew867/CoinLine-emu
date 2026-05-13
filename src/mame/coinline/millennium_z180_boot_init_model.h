// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <vector>

namespace coinline::z180 {

struct boot_init_config {
	bool banked_mode = true;
	bool battery_backed = true;
	std::uint16_t stack_addr = 0x0000;
	std::uint16_t x_mem_top_addr = 0x80FE;
	std::uint8_t cbr_value = 0x00;
	std::uint8_t cbar_value = 0x00;
};

class boot_init_model {
public:
	void configure(boot_init_config const &cfg) noexcept { m_cfg = cfg; }
	void reset() noexcept;

	void set_bss_size(std::size_t bytes);
	void set_mem_top(std::uint8_t v) noexcept { m_mem_top = v; }

	void run_cstart() noexcept;
	void run_bss_clear_and_malloc_seed() noexcept;
	void run_pre_reset_handoff() noexcept;
	bool detect_mem_top_corruption(std::uint8_t corrupt_marker) const noexcept;

	bool bank_init_called() const noexcept { return m_bank_init_called; }
	unsigned bank_init_call_count() const noexcept { return m_bank_init_call_count; }
	bool jumped_xreset1() const noexcept { return m_jumped_xreset1; }
	bool non_banked_mmu_configured() const noexcept { return m_non_banked_mmu_configured; }
	std::uint16_t sp() const noexcept { return m_sp; }
	std::uint8_t freep() const noexcept { return m_freep; }
	std::uint8_t heap_base() const noexcept { return m_heap_base; }
	std::uint8_t cbr() const noexcept { return m_cbr; }
	std::uint8_t cbar() const noexcept { return m_cbar; }
	std::vector<std::uint8_t> const &bss() const noexcept { return m_bss; }

private:
	boot_init_config m_cfg{};
	std::uint16_t m_sp = 0U;
	std::uint8_t m_freep = 0xFFU;
	std::uint8_t m_heap_base = 0xFFU;
	std::uint8_t m_cbr = 0x00U;
	std::uint8_t m_cbar = 0x00U;
	std::uint8_t m_mem_top = 0x00U;
	bool m_bank_init_called = false;
	unsigned m_bank_init_call_count = 0U;
	bool m_non_banked_mmu_configured = false;
	bool m_jumped_xreset1 = false;
	std::vector<std::uint8_t> m_bss;
};

} // namespace coinline::z180

