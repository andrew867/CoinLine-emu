// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "millennium_am8048_core_contract.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

class millennium_am8048_core : public coinline::tp8048::am8048_core_contract
{
public:
	void configure(coinline::tp8048::am8048_config const &cfg, coinline::tp8048::am8048_callbacks cb) override;
	void load_program_rom(std::uint8_t const *data, std::size_t size) override;
	void reset() override;
	void step_cycles(std::uint64_t cycles) override;
	std::uint64_t cycles() const override { return m_cycles_executed; }

	void assert_external_interrupt(bool level) override { m_ext_irq = level; }
	void assert_timer_interrupt(bool level) override { m_timer_irq = level; }

	std::uint8_t debug_port_latch(unsigned port) const override;
	std::uint16_t debug_pc() const override { return m_pc; }

private:
	unsigned execute_one();
	std::uint8_t fetch();
	std::uint8_t read_reg(unsigned r) const;
	void write_reg(unsigned r, std::uint8_t v);
	std::uint8_t read_ram(std::uint8_t addr) const;
	void write_ram(std::uint8_t addr, std::uint8_t v);
	std::uint8_t read_indirect(unsigned r) const;
	void write_indirect(unsigned r, std::uint8_t v);
	void push_state();
	void pop_state(bool retr);
	void jump_page(std::uint8_t opcode, std::uint8_t low);
	void apply_alu_flags_add(std::uint8_t a, std::uint8_t b, std::uint16_t res);
	void trace_event(char const *event, std::uint8_t value) const;

	coinline::tp8048::am8048_config m_cfg{};
	coinline::tp8048::am8048_callbacks m_cb{};
	std::vector<std::uint8_t> m_rom;
	std::array<std::uint8_t, 224> m_ram{};
	std::array<std::uint8_t, 8> m_port_latch{ 0xffU, 0xffU, 0xffU, 0xffU, 0xffU, 0xffU, 0xffU, 0xffU };
	std::uint8_t m_a = 0U;
	std::uint8_t m_psw = 0x08U;
	std::uint8_t m_t = 0U;
	std::uint8_t m_flags_f1 = 0U;
	bool m_flag_mb = false;
	bool m_in_irq = false;
	bool m_i_ena = false;
	bool m_t_ena = false;
	bool m_t_req = false;
	bool m_t_flag = false;
	bool m_tmr_on = false;
	bool m_tmr_mode_counter = false;
	bool m_t0_clk_ena = false;
	bool m_stop_on_stop_tcnt = false;
	bool m_idle_mode = false;
	bool m_stop_mode = false;
	std::uint8_t m_prescaler = 0U;
	std::uint8_t m_t1_prev = 0U;
	std::uint64_t m_cycles_executed = 0ULL;
	std::uint16_t m_pc = 0U;
	bool m_ext_irq = false;
	bool m_timer_irq = false;
};
