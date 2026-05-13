// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_security_model.h"

void millennium_security_model::configure(millennium_security_board_config const &cfg)
{
	m_cfg = cfg;
	if (m_cfg.debounce_cycles < 0)
		m_cfg.debounce_cycles = 0;
}

void millennium_security_model::reset()
{
	m_stable = 0;
	m_last_raw = 0;
	m_last_change_cycle = 0;
}

std::uint8_t millennium_security_model::read_lines(std::uint32_t sec_ioport_bits, std::uint64_t cycle)
{
	std::uint8_t const raw = static_cast<std::uint8_t>(sec_ioport_bits & 0x0fU);
	if (raw != m_last_raw) {
		m_last_raw = raw;
		m_last_change_cycle = cycle;
	}
	if (m_cfg.debounce_cycles <= 0 || cycle >= m_last_change_cycle + std::uint64_t(m_cfg.debounce_cycles))
		m_stable = raw;
	return m_stable & 0x0fU;
}
