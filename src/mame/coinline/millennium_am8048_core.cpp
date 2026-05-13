// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_am8048_core.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <utility>

void millennium_am8048_core::configure(coinline::tp8048::am8048_config const &cfg, coinline::tp8048::am8048_callbacks cb)
{
	m_cfg = cfg;
	m_cb = std::move(cb);
	char const *stop_mode = std::getenv("COINLINE_TP8048_STOP_ON_STOP_TCNT");
	m_stop_on_stop_tcnt = (stop_mode && *stop_mode && *stop_mode != '0');
}

void millennium_am8048_core::load_program_rom(std::uint8_t const *data, std::size_t size)
{
	m_rom.assign(4096U, 0x00U);
	if (data && size > 0U)
		std::memcpy(m_rom.data(), data, std::min<std::size_t>(size, m_rom.size()));
}

void millennium_am8048_core::reset()
{
	m_ram.fill(0U);
	m_port_latch.fill(0xffU);
	m_a = 0U;
	m_psw = 0x08U;
	m_t = 0U;
	m_flags_f1 = 0U;
	m_flag_mb = false;
	m_in_irq = false;
	m_i_ena = false;
	m_t_ena = false;
	m_t_req = false;
	m_t_flag = false;
	m_tmr_on = false;
	m_tmr_mode_counter = false;
	m_t0_clk_ena = false;
	m_idle_mode = false;
	m_stop_mode = false;
	m_prescaler = 0U;
	m_t1_prev = static_cast<std::uint8_t>((m_cb.read_test_input && m_cb.read_test_input(1U)) ? 1U : 0U);
	m_cycles_executed = 0ULL;
	m_pc = 0U;
	m_ext_irq = false;
	m_timer_irq = false;
}

void millennium_am8048_core::step_cycles(std::uint64_t cycles)
{
	while (cycles > 0U) {
		unsigned const spent = execute_one();
		if (spent == 0U)
			break;
		if (cycles > spent)
			cycles -= spent;
		else
			cycles = 0U;
	}
}

std::uint8_t millennium_am8048_core::debug_port_latch(unsigned port) const
{
	if (port < m_port_latch.size())
		return m_port_latch[port];
	return 0xffU;
}

std::uint8_t millennium_am8048_core::fetch()
{
	std::uint16_t const addr = static_cast<std::uint16_t>(m_pc & 0x0fffU);
	m_pc = static_cast<std::uint16_t>((m_pc + 1U) & 0x0fffU);
	return (addr < m_rom.size()) ? m_rom[addr] : 0x00U;
}

std::uint8_t millennium_am8048_core::read_reg(unsigned r) const
{
	unsigned const bank = (m_psw & 0x10U) ? 24U : 0U;
	return read_ram(static_cast<std::uint8_t>(bank + (r & 7U)));
}

void millennium_am8048_core::write_reg(unsigned r, std::uint8_t v)
{
	unsigned const bank = (m_psw & 0x10U) ? 24U : 0U;
	write_ram(static_cast<std::uint8_t>(bank + (r & 7U)), v);
}

std::uint8_t millennium_am8048_core::read_ram(std::uint8_t addr) const
{
	return m_ram[static_cast<std::size_t>(addr) % m_ram.size()];
}

void millennium_am8048_core::write_ram(std::uint8_t addr, std::uint8_t v)
{
	m_ram[static_cast<std::size_t>(addr) % m_ram.size()] = v;
}

std::uint8_t millennium_am8048_core::read_indirect(unsigned r) const
{
	return read_ram(read_reg(r & 1U));
}

void millennium_am8048_core::write_indirect(unsigned r, std::uint8_t v)
{
	write_ram(read_reg(r & 1U), v);
}

void millennium_am8048_core::push_state()
{
	std::uint8_t const sp = static_cast<std::uint8_t>((m_psw & 0x07U) * 2U + 8U);
	std::uint16_t const state = static_cast<std::uint16_t>(((m_psw << 8U) & 0xf000U) | (m_pc & 0x0fffU));
	write_ram(static_cast<std::uint8_t>(sp + 0U), static_cast<std::uint8_t>(state & 0xffU));
	write_ram(static_cast<std::uint8_t>(sp + 1U), static_cast<std::uint8_t>(state >> 8U));
	m_psw = static_cast<std::uint8_t>((m_psw & 0xf8U) | ((m_psw + 1U) & 0x07U));
}

void millennium_am8048_core::pop_state(bool retr)
{
	m_psw = static_cast<std::uint8_t>((m_psw & 0xf8U) | ((m_psw - 1U) & 0x07U));
	std::uint8_t const sp = static_cast<std::uint8_t>((m_psw & 0x07U) * 2U + 8U);
	std::uint16_t const state = static_cast<std::uint16_t>((read_ram(static_cast<std::uint8_t>(sp + 1U)) << 8U)
		| read_ram(static_cast<std::uint8_t>(sp + 0U)));
	m_pc = static_cast<std::uint16_t>(state & 0x0fffU);
	if (retr)
		m_psw = static_cast<std::uint8_t>((m_psw & 0x0fU) | ((state >> 8U) & 0xf0U));
}

void millennium_am8048_core::jump_page(std::uint8_t opcode, std::uint8_t low)
{
	std::uint16_t const hi = static_cast<std::uint16_t>((opcode << 3U) & 0x0700U);
	std::uint16_t const bank = (m_flag_mb && !m_in_irq) ? 0x0800U : 0x0000U;
	m_pc = static_cast<std::uint16_t>(hi | low | bank);
}

void millennium_am8048_core::apply_alu_flags_add(std::uint8_t a, std::uint8_t b, std::uint16_t res)
{
	if ((res & 0x100U) != 0U)
		m_psw |= 0x80U;
	else
		m_psw &= static_cast<std::uint8_t>(~0x80U);
	std::uint8_t const ac = static_cast<std::uint8_t>((((a & b) | (a & ~res) | (b & ~res)) & 0x08U) ? 1U : 0U);
	if (ac)
		m_psw |= 0x40U;
	else
		m_psw &= static_cast<std::uint8_t>(~0x40U);
}

void millennium_am8048_core::trace_event(char const *event, std::uint8_t value) const
{
	if (m_cb.trace)
		m_cb.trace(event, m_cycles_executed, value);
}

unsigned millennium_am8048_core::execute_one()
{
	unsigned cycles = 1U;
	if (!m_t_ena)
		m_t_req = false;
	if (!m_in_irq && ((m_i_ena && m_ext_irq) || (m_t_ena && (m_t_req || m_timer_irq)))) {
		push_state();
		m_pc = (m_i_ena && m_ext_irq) ? 0x0003U : 0x0007U;
		m_in_irq = true;
		if (m_pc == 0x0007U)
			m_t_req = false;
		cycles += 2U;
		trace_event((m_pc == 0x0003U) ? "irq_enter_ext" : "irq_enter_timer", static_cast<std::uint8_t>(m_pc & 0xffU));
		m_idle_mode = false;
		m_stop_mode = false;
	}
	if (m_stop_mode) {
		m_cycles_executed += cycles;
		return cycles;
	}
	if (m_idle_mode) {
		for (unsigned i = 0; i < cycles; ++i) {
			m_prescaler = static_cast<std::uint8_t>((m_prescaler + 1U) & 31U);
			if (m_tmr_on && !m_tmr_mode_counter && m_prescaler == 0U) {
				m_t = static_cast<std::uint8_t>(m_t + 1U);
				if (m_t == 0U) {
					m_t_req = true;
					m_t_flag = true;
				}
			}
			m_cycles_executed++;
		}
		return cycles;
	}
	std::uint8_t const op = fetch();
	switch (op) {
	case 0x00: /* NOP */ break;
	case 0x01:
		m_idle_mode = true;
		trace_event("idle_enter", op);
		break; // IDL
	case 0x05: m_i_ena = true; break;   // EN I
	case 0x15: m_i_ena = false; break;  // DIS I
	case 0x25: m_t_ena = true; break;   // EN TCNTI
	case 0x35: m_t_ena = false; break;  // DIS TCNTI
	case 0x83:
		pop_state(false);
		cycles++;
		trace_event("ret", op);
		break; // RET
	case 0x93:
		pop_state(true);
		m_in_irq = false;
		cycles++;
		trace_event("isr_return", op);
		break; // RETR
	case 0x07: m_a = static_cast<std::uint8_t>(m_a - 1U); break; // DEC A
	case 0x17: m_a = static_cast<std::uint8_t>(m_a + 1U); break; // INC A
	case 0x27: m_a = 0U; break; // CLR A
	case 0x37: m_a ^= 0xffU; break; // CPL A
	case 0x42: m_a = m_t; break; // MOV A,T
	case 0x62: m_t = m_a; break; // MOV T,A
	case 0xC7: m_a = m_psw; break; // MOV A,PSW
	case 0xD7: m_psw = static_cast<std::uint8_t>(m_a | 0x08U); break; // MOV PSW,A
	case 0x97: m_psw &= static_cast<std::uint8_t>(~0x80U); break; // CLR C
	case 0xA7: m_psw ^= 0x80U; break; // CPL C
	case 0x85: m_psw &= static_cast<std::uint8_t>(~0x20U); break; // CLR F0
	case 0x95: m_psw ^= 0x20U; break; // CPL F0
	case 0xA5: m_flags_f1 = 0U; break; // CLR F1
	case 0xB5: m_flags_f1 ^= 1U; break; // CPL F1
	case 0x47: m_a = static_cast<std::uint8_t>((m_a << 4U) | (m_a >> 4U)); break; // SWAP A
	case 0x77: m_a = static_cast<std::uint8_t>((m_a >> 1U) | (m_a << 7U)); break; // RR A
	case 0xE7: m_a = static_cast<std::uint8_t>((m_a << 1U) | (m_a >> 7U)); break; // RL A
	case 0x67: { // RRC A
		std::uint8_t const c = (m_psw & 0x80U) ? 0x80U : 0x00U;
		bool const new_c = (m_a & 0x01U) != 0U;
		m_a = static_cast<std::uint8_t>((m_a >> 1U) | c);
		m_psw = static_cast<std::uint8_t>((m_psw & ~0x80U) | (new_c ? 0x80U : 0x00U));
		break;
	}
	case 0xF7: { // RLC A
		std::uint8_t const c = (m_psw & 0x80U) ? 1U : 0U;
		bool const new_c = (m_a & 0x80U) != 0U;
		m_a = static_cast<std::uint8_t>((m_a << 1U) | c);
		m_psw = static_cast<std::uint8_t>((m_psw & ~0x80U) | (new_c ? 0x80U : 0x00U));
		break;
	}
	case 0xE5: m_flag_mb = false; break; // SEL MB0
	case 0xF5: m_flag_mb = true; break;  // SEL MB1
	case 0xC5: m_psw &= static_cast<std::uint8_t>(~0x10U); break; // SEL RB0
	case 0xD5: m_psw |= 0x10U; break; // SEL RB1
	case 0x55: m_tmr_mode_counter = false; m_tmr_on = true; m_prescaler = 0U; break; // STRT T
	case 0x45: m_tmr_mode_counter = true; m_tmr_on = true; break; // STRT CNT
	case 0x65:
		m_tmr_on = false; // STOP TCNT
		if (m_stop_on_stop_tcnt)
			m_stop_mode = true;
		trace_event("stop_tcnt", op);
		break;
	case 0x75: m_t0_clk_ena = true; break; // ENT0 CLK
	case 0x08: { // INS A,BUS — quasi-bidirectional: pin reads external ∧ latch (latch 0 forces low).
		cycles++;
		std::uint8_t const ext = m_cb.read_port ? m_cb.read_port(0U) : 0xffU;
		m_a = static_cast<std::uint8_t>(ext & m_port_latch[0]);
		trace_event("ins_bus", m_a);
		break;
	}
	case 0x09:
	case 0x0A: { // IN A,P1/P2
		cycles++;
		unsigned const p = op - 0x08U;
		std::uint8_t const ext = m_cb.read_port ? m_cb.read_port(p) : 0xffU;
		m_a = static_cast<std::uint8_t>(ext & m_port_latch[p]);
		trace_event("in_port", m_a);
		break;
	}
	case 0x0C:
	case 0x0D:
	case 0x0E:
	case 0x0F: { // MOVD A,P4..P7 — low nibble from expander pins ∧ latch; upper A bits cleared.
		cycles++;
		unsigned const p = static_cast<unsigned>(4U + (op - 0x0CU));
		std::uint8_t const ext = m_cb.read_port ? m_cb.read_port(p) : 0xffU;
		m_a = static_cast<std::uint8_t>((ext & m_port_latch[p]) & 0x0fU);
		trace_event("movd_a_p", m_a);
		break;
	}
	case 0x02:
	case 0x39:
	case 0x3A: { // OUTL BUS/P1/P2,A
		cycles++;
		unsigned const p = (op == 0x02) ? 0U : (op - 0x38U);
		m_port_latch[p] = m_a;
		if (m_cb.write_port)
			m_cb.write_port(p, m_a);
		trace_event("out_port", m_a);
		break;
	}
	case 0x3C:
	case 0x3D:
	case 0x3E:
	case 0x3F: { // MOVD P4..P7,A
		cycles++;
		unsigned const p = static_cast<unsigned>(4U + (op - 0x3CU));
		m_port_latch[p] = m_a;
		if (m_cb.write_port)
			m_cb.write_port(p, m_a);
		if (m_cb.tone_registers_changed)
			m_cb.tone_registers_changed(m_port_latch[4], m_port_latch[5]);
		trace_event("movd_p_a", m_a);
		break;
	}
	default:
		if ((op & 0xf8U) == 0xb8U) { // MOV Rn,#imm
			cycles++;
			write_reg(op & 7U, fetch());
		} else if ((op & 0xf8U) == 0xa8U) { // MOV Rn,A
			write_reg(op & 7U, m_a);
		} else if ((op & 0xf8U) == 0xf8U) { // MOV A,Rn
			m_a = read_reg(op & 7U);
		} else if ((op & 0xfeU) == 0x80U) { // MOVX A,@Rn
			cycles++;
			std::uint16_t const addr = static_cast<std::uint16_t>(((m_port_latch[2] << 8U) & 0x0f00U) | read_reg(op & 1U));
			m_a = m_cb.read_ext_data ? m_cb.read_ext_data(addr) : 0xffU;
			trace_event("movx_read", m_a);
		} else if ((op & 0xfeU) == 0x90U) { // MOVX @Rn,A
			cycles++;
			std::uint16_t const addr = static_cast<std::uint16_t>(((m_port_latch[2] << 8U) & 0x0f00U) | read_reg(op & 1U));
			if (m_cb.write_ext_data)
				m_cb.write_ext_data(addr, m_a);
			trace_event("movx_write", m_a);
		} else if ((op & 0xfeU) == 0xa0U) { // MOV @Rn,A
			write_indirect(op & 1U, m_a);
		} else if ((op & 0xfeU) == 0xf0U) { // MOV A,@Rn
			m_a = read_indirect(op & 1U);
		} else if ((op & 0xfeU) == 0xb0U) { // MOV @Rn,#imm
			cycles++;
			write_indirect(op & 1U, fetch());
		} else if ((op & 0xfeU) == 0xc0U) { // DEC @Rn
			write_indirect(op & 1U, static_cast<std::uint8_t>(read_indirect(op & 1U) - 1U));
		} else if ((op & 0xfeU) == 0x10U) { // INC @Rn
			write_indirect(op & 1U, static_cast<std::uint8_t>(read_indirect(op & 1U) + 1U));
		} else if ((op & 0xf8U) == 0x18U) { // INC Rn
			write_reg(op & 7U, static_cast<std::uint8_t>(read_reg(op & 7U) + 1U));
		} else if ((op & 0xf8U) == 0xc8U) { // DEC Rn
			write_reg(op & 7U, static_cast<std::uint8_t>(read_reg(op & 7U) - 1U));
		} else if (op == 0x23U) { // MOV A,#imm
			cycles++;
			m_a = fetch();
		} else if (op == 0x03U) { // ADD A,#imm
			cycles++;
			std::uint8_t const arg = fetch();
			std::uint16_t const res = static_cast<std::uint16_t>(m_a + arg);
			apply_alu_flags_add(m_a, arg, res);
			m_a = static_cast<std::uint8_t>(res & 0xffU);
		} else if (op == 0x13U) { // ADDC A,#imm
			cycles++;
			std::uint8_t const arg = fetch();
			std::uint8_t const c = (m_psw & 0x80U) ? 1U : 0U;
			std::uint16_t const res = static_cast<std::uint16_t>(m_a + arg + c);
			apply_alu_flags_add(m_a, static_cast<std::uint8_t>(arg + c), res);
			m_a = static_cast<std::uint8_t>(res & 0xffU);
		} else if (op == 0x43U) { // ORL A,#imm
			cycles++;
			m_a = static_cast<std::uint8_t>(m_a | fetch());
		} else if (op == 0x88U || op == 0x89U || op == 0x8AU) { // ORL BUS/P1/P2,#imm
			cycles++;
			unsigned const p = op - 0x88U;
			std::uint8_t const imm = fetch();
			m_port_latch[p] = static_cast<std::uint8_t>(m_port_latch[p] | imm);
			if (m_cb.write_port)
				m_cb.write_port(p, m_port_latch[p]);
			trace_event("orl_port_imm", m_port_latch[p]);
		} else if (op == 0x53U) { // ANL A,#imm
			cycles++;
			m_a = static_cast<std::uint8_t>(m_a & fetch());
		} else if (op == 0x98U || op == 0x99U || op == 0x9AU) { // ANL BUS/P1/P2,#imm
			cycles++;
			unsigned const p = op - 0x98U;
			std::uint8_t const imm = fetch();
			m_port_latch[p] = static_cast<std::uint8_t>(m_port_latch[p] & imm);
			if (m_cb.write_port)
				m_cb.write_port(p, m_port_latch[p]);
			trace_event("anl_port_imm", m_port_latch[p]);
		} else if ((op & 0xf8U) == 0x58U) { // ANL A,Rn
			m_a = static_cast<std::uint8_t>(m_a & read_reg(op & 7U));
		} else if ((op & 0xfeU) == 0x50U) { // ANL A,@Rn
			m_a = static_cast<std::uint8_t>(m_a & read_indirect(op & 1U));
		} else if (op == 0xd3U) { // XRL A,#imm
			cycles++;
			m_a = static_cast<std::uint8_t>(m_a ^ fetch());
		} else if ((op & 0xf8U) == 0xd8U) { // XRL A,Rn
			m_a = static_cast<std::uint8_t>(m_a ^ read_reg(op & 7U));
		} else if ((op & 0xfeU) == 0xd0U) { // XRL A,@Rn
			m_a = static_cast<std::uint8_t>(m_a ^ read_indirect(op & 1U));
		} else if ((op & 0xf8U) == 0x48U) { // ORL A,Rn
			m_a = static_cast<std::uint8_t>(m_a | read_reg(op & 7U));
		} else if ((op & 0xfeU) == 0x40U) { // ORL A,@Rn
			m_a = static_cast<std::uint8_t>(m_a | read_indirect(op & 1U));
		} else if ((op & 0xf8U) == 0x68U) { // ADD A,Rn
			std::uint8_t const arg = read_reg(op & 7U);
			std::uint16_t const res = static_cast<std::uint16_t>(m_a + arg);
			apply_alu_flags_add(m_a, arg, res);
			m_a = static_cast<std::uint8_t>(res & 0xffU);
		} else if ((op & 0xfeU) == 0x60U) { // ADD A,@Rn
			std::uint8_t const arg = read_indirect(op & 1U);
			std::uint16_t const res = static_cast<std::uint16_t>(m_a + arg);
			apply_alu_flags_add(m_a, arg, res);
			m_a = static_cast<std::uint8_t>(res & 0xffU);
		} else if ((op & 0xf8U) == 0x78U) { // ADDC A,Rn
			std::uint8_t const arg = read_reg(op & 7U);
			std::uint8_t const c = (m_psw & 0x80U) ? 1U : 0U;
			std::uint16_t const res = static_cast<std::uint16_t>(m_a + arg + c);
			apply_alu_flags_add(m_a, static_cast<std::uint8_t>(arg + c), res);
			m_a = static_cast<std::uint8_t>(res & 0xffU);
		} else if ((op & 0xfeU) == 0x70U) { // ADDC A,@Rn
			std::uint8_t const arg = read_indirect(op & 1U);
			std::uint8_t const c = (m_psw & 0x80U) ? 1U : 0U;
			std::uint16_t const res = static_cast<std::uint16_t>(m_a + arg + c);
			apply_alu_flags_add(m_a, static_cast<std::uint8_t>(arg + c), res);
			m_a = static_cast<std::uint8_t>(res & 0xffU);
		} else if (op == 0x04U || op == 0x24U || op == 0x44U || op == 0x64U
			|| op == 0x84U || op == 0xa4U || op == 0xc4U || op == 0xe4U) { // JMP
			cycles++;
			jump_page(op, fetch());
		} else if (op == 0x14U || op == 0x34U || op == 0x54U || op == 0x74U
			|| op == 0x94U || op == 0xb4U || op == 0xd4U || op == 0xf4U) { // CALL
			cycles++;
			std::uint8_t const low = fetch();
			push_state();
			jump_page(op, low);
		} else if (op == 0x96U || op == 0xc6U) { // JNZ/JZ
			cycles++;
			std::uint8_t const addr = fetch();
			bool const take = (op == 0x96U) ? (m_a != 0U) : (m_a == 0U);
			if (take)
				m_pc = static_cast<std::uint16_t>((m_pc & 0xff00U) | addr);
		} else if (op == 0x12U || op == 0x32U || op == 0x52U || op == 0x72U
			|| op == 0x92U || op == 0xB2U || op == 0xD2U || op == 0xF2U) { // JB0..JB7
			cycles++;
			std::uint8_t const addr = fetch();
			unsigned const bit = static_cast<unsigned>((op >> 5U) & 0x07U);
			if ((m_a & static_cast<std::uint8_t>(1U << bit)) != 0U)
				m_pc = static_cast<std::uint16_t>((m_pc & 0xff00U) | addr);
		} else if (op == 0x16U) { // JTF
			cycles++;
			std::uint8_t const addr = fetch();
			if (m_t_flag) {
				m_pc = static_cast<std::uint16_t>((m_pc & 0xff00U) | addr);
				m_t_flag = false;
			}
		} else if (op == 0xA3U) { // MOVP A,@A
			cycles++;
			std::uint16_t const page = static_cast<std::uint16_t>(m_pc & 0x0f00U);
			std::uint16_t const addr = static_cast<std::uint16_t>(page | m_a);
			m_a = (addr < m_rom.size()) ? m_rom[addr] : 0x00U;
		} else if (op == 0xB3U) { // JMPP @A
			cycles++;
			std::uint16_t const page = static_cast<std::uint16_t>(m_pc & 0x0f00U);
			std::uint16_t const vec_addr = static_cast<std::uint16_t>(page | m_a);
			std::uint8_t const low = (vec_addr < m_rom.size()) ? m_rom[vec_addr] : 0x00U;
			m_pc = static_cast<std::uint16_t>(page | low);
		} else if (op == 0xE3U) { // MOVP3 A,@A
			cycles++;
			std::uint16_t const addr = static_cast<std::uint16_t>(0x0300U | m_a);
			m_a = (addr < m_rom.size()) ? m_rom[addr] : 0x00U;
		} else if (op == 0x8CU || op == 0x8DU || op == 0x8EU || op == 0x8FU) { // ORLD P4..P7,A
			cycles++;
			unsigned const p = static_cast<unsigned>(4U + (op - 0x8CU));
			m_port_latch[p] = static_cast<std::uint8_t>((m_port_latch[p] & 0xF0U) | ((m_port_latch[p] | m_a) & 0x0FU));
			if (m_cb.write_port)
				m_cb.write_port(p, m_port_latch[p]);
			if (m_cb.tone_registers_changed)
				m_cb.tone_registers_changed(m_port_latch[4], m_port_latch[5]);
			trace_event("orld_p_a", m_port_latch[p]);
		} else if (op == 0x9CU || op == 0x9DU || op == 0x9EU || op == 0x9FU) { // ANLD P4..P7,A
			cycles++;
			unsigned const p = static_cast<unsigned>(4U + (op - 0x9CU));
			m_port_latch[p] = static_cast<std::uint8_t>((m_port_latch[p] & 0xF0U) | ((m_port_latch[p] & m_a) & 0x0FU));
			if (m_cb.write_port)
				m_cb.write_port(p, m_port_latch[p]);
			if (m_cb.tone_registers_changed)
				m_cb.tone_registers_changed(m_port_latch[4], m_port_latch[5]);
			trace_event("anld_p_a", m_port_latch[p]);
		} else if (op == 0xf6U || op == 0xe6U) { // JC/JNC
			cycles++;
			std::uint8_t const addr = fetch();
			bool const c = (m_psw & 0x80U) != 0U;
			bool const take = (op == 0xf6U) ? c : !c;
			if (take)
				m_pc = static_cast<std::uint16_t>((m_pc & 0xff00U) | addr);
		} else if (op == 0xB6U || op == 0x76U) { // JF0 / JF1
			cycles++;
			std::uint8_t const addr = fetch();
			bool const take = (op == 0xB6U) ? ((m_psw & 0x20U) != 0U) : (m_flags_f1 != 0U);
			if (take)
				m_pc = static_cast<std::uint16_t>((m_pc & 0xff00U) | addr);
		} else if (op == 0x86U) { // JNI
			cycles++;
			std::uint8_t const addr = fetch();
			if (m_ext_irq)
				m_pc = static_cast<std::uint16_t>((m_pc & 0xff00U) | addr);
		} else if (op == 0x26U || op == 0x36U || op == 0x46U || op == 0x56U) { // JT/JNT0/1
			cycles++;
			std::uint8_t const addr = fetch();
			bool const t0 = m_cb.read_test_input ? m_cb.read_test_input(0U) : false;
			bool const t1 = m_cb.read_test_input ? m_cb.read_test_input(1U) : false;
			bool take = false;
			if (op == 0x26U) take = !t0;
			else if (op == 0x36U) take = t0;
			else if (op == 0x46U) take = !t1;
			else take = t1;
			if (take)
				m_pc = static_cast<std::uint16_t>((m_pc & 0xff00U) | addr);
		} else if ((op & 0xf8U) == 0xe8U) { // DJNZ Rn,addr
			cycles++;
			std::uint8_t const addr = fetch();
			std::uint8_t const v = static_cast<std::uint8_t>(read_reg(op & 7U) - 1U);
			write_reg(op & 7U, v);
			if (v != 0U)
				m_pc = static_cast<std::uint16_t>((m_pc & 0xff00U) | addr);
		} else if ((op & 0xf8U) == 0x28U) { // XCH A,Rn
			std::uint8_t const rn = read_reg(op & 7U);
			write_reg(op & 7U, m_a);
			m_a = rn;
		} else if ((op & 0xfeU) == 0x20U) { // XCH A,@Rn
			std::uint8_t const memv = read_indirect(op & 1U);
			write_indirect(op & 1U, m_a);
			m_a = memv;
		} else if ((op & 0xfeU) == 0x30U) { // XCHD A,@Rn
			std::uint8_t const memv = read_indirect(op & 1U);
			write_indirect(op & 1U, static_cast<std::uint8_t>((memv & 0xf0U) | (m_a & 0x0fU)));
			m_a = static_cast<std::uint8_t>((m_a & 0xf0U) | (memv & 0x0fU));
		} else if (op == 0x57U) { // DA A
			std::uint8_t adjust = 0U;
			if ((m_psw & 0x40U) || (m_a & 0x0fU) > 9U)
				adjust = static_cast<std::uint8_t>(adjust + 0x06U);
			if ((m_psw & 0x80U) || (m_a >> 4U) > 9U || (((m_a & 0x0fU) > 9U) && ((m_a >> 4U) == 9U))) {
				adjust = static_cast<std::uint8_t>(adjust + 0x60U);
				m_psw |= 0x80U;
			}
			m_a = static_cast<std::uint8_t>(m_a + adjust);
		} else {
			trace_event("unsupported_opcode", op);
		}
		break;
	}

	for (unsigned i = 0; i < cycles; ++i) {
		m_prescaler = static_cast<std::uint8_t>((m_prescaler + 1U) & 31U);
		bool const t1_now = (m_cb.read_test_input && m_cb.read_test_input(1U));
		if (m_tmr_on) {
			bool tmr_inc = false;
			if (m_tmr_mode_counter) {
				if (!t1_now && m_t1_prev)
					tmr_inc = true; // falling edge
			} else if (m_prescaler == 0U) {
				tmr_inc = true;
			}
			if (tmr_inc) {
				m_t = static_cast<std::uint8_t>(m_t + 1U);
				if (m_t == 0U) {
					m_t_req = true;
					m_t_flag = true;
				}
			}
		}
		m_t1_prev = static_cast<std::uint8_t>(t1_now ? 1U : 0U);
		m_cycles_executed++;
	}
	return cycles;
}
