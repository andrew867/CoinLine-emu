// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_coin_model.h"

namespace {

std::uint64_t us_to_cycles(unsigned us, std::uint64_t cpu_hz)
{
	return std::uint64_t(us) * (cpu_hz / 1000000ULL);
}

} // namespace

void millennium_coin_model::configure(millennium_coin_board_config const &cfg)
{
	m_cfg = cfg;
	if (m_cfg.pulse_width_us == 0)
		m_cfg.pulse_width_us = 350;
	if (m_cfg.inter_pulse_gap_us == 0)
		m_cfg.inter_pulse_gap_us = 350;
}

void millennium_coin_model::reset()
{
	m_disabled = false;
	m_jam_latched = false;
	m_reject_route = false;
	m_emitting = false;
	m_pending_cents = 0;
	m_escrow_cents = 0;
	m_pulse_target = 0;
	m_train_start_cycle = 0;
	m_protocol_timeout_count = 0U;
	m_protocol_reset_pulse_count = 0U;
}

bool millennium_coin_model::denom_allowed(int cents) const
{
	for (int d : m_cfg.denominations_cents)
		if (d == cents)
			return true;
	return false;
}

int millennium_coin_model::pulses_for_valid_denom(int cents) const
{
	if (cents <= 0 || (cents % 5) != 0)
		return -1;
	return cents / 5;
}

std::uint64_t millennium_coin_model::pulse_width_cycles(std::uint64_t cpu_hz) const
{
	return us_to_cycles(m_cfg.pulse_width_us, cpu_hz ? cpu_hz : m_cpu_hz);
}

std::uint64_t millennium_coin_model::inter_pulse_gap_cycles(std::uint64_t cpu_hz) const
{
	return us_to_cycles(m_cfg.inter_pulse_gap_us, cpu_hz ? cpu_hz : m_cpu_hz);
}

void millennium_coin_model::write_control(std::uint8_t data, std::uint64_t cycle)
{
	(void)cycle;
	if (data & 0x01U)
		m_disabled = true;
	else
		m_disabled = false;
	if (data & 0x02U) {
		m_jam_latched = false;
		m_reject_route = false;
	}
	if (data & 0x04U)
		m_reject_route = false;
}

bool millennium_coin_model::begin_insert_cents(int cents, std::uint64_t cycle, std::uint64_t cpu_hz)
{
	if (m_disabled || m_jam_latched)
		return false;
	if (m_cfg.validator_type != "pulse" && m_cfg.validator_type != "uart")
		return false;
	if (!denom_allowed(cents))
		return false;
	int const n = pulses_for_valid_denom(cents);
	if (n <= 0)
		return false;
	m_cpu_hz = cpu_hz ? cpu_hz : 12288000;
	m_pending_cents = cents;
	m_escrow_cents = cents;
	m_pulse_target = static_cast<unsigned>(n);
	m_emitting = true;
	m_train_start_cycle = cycle;
	m_reject_route = false;
	return true;
}

void millennium_coin_model::inject_jam(std::uint64_t cycle)
{
	(void)cycle;
	m_jam_latched = true;
	m_emitting = false;
	m_pending_cents = 0;
	m_escrow_cents = 0;
	m_pulse_target = 0;
}

void millennium_coin_model::inject_reject_route(std::uint64_t cycle)
{
	(void)cycle;
	m_reject_route = true;
	m_emitting = false;
	m_pending_cents = 0;
	m_escrow_cents = 0;
	m_pulse_target = 0;
}

std::uint8_t millennium_coin_model::read_status(std::uint64_t cycle)
{
	bool pulse_line = false;
	if (m_emitting && !m_jam_latched && m_pulse_target > 0) {
		std::uint64_t const pw = pulse_width_cycles(m_cpu_hz);
		std::uint64_t const gap = inter_pulse_gap_cycles(m_cpu_hz);
		std::uint64_t const n = std::uint64_t(m_pulse_target);
		std::uint64_t const total = n * pw + (n > 0 ? (n - 1U) : 0U) * gap;
		if (cycle >= m_train_start_cycle + total) {
			m_emitting = false;
			m_pending_cents = 0;
		} else {
			for (unsigned i = 0; i < m_pulse_target; ++i) {
				std::uint64_t const t0 = m_train_start_cycle + std::uint64_t(i) * (pw + gap);
				if (cycle >= t0 && cycle < t0 + pw) {
					pulse_line = true;
					break;
				}
			}
		}
	}

	std::uint8_t s = 0;
	if (pulse_line)
		s |= 0x01U;
	if (!m_disabled && !m_jam_latched)
		s |= 0x02U;
	if (m_jam_latched)
		s |= 0x04U;
	if (m_reject_route)
		s |= 0x08U;
	if (m_disabled)
		s |= 0x10U;
	return s;
}

bool millennium_coin_model::protocol_handle_command(protocol_command cmd, protocol_status_frame &status_out) noexcept
{
	status_out.code = 0x42U;
	status_out.fault = m_jam_latched ? 0x01U : 0x00U;
	status_out.sensor = m_escrow_cents > 0 ? 0x01U : 0x00U;

	switch (cmd) {
	case protocol_command::wake:
		m_disabled = false;
		return true;
	case protocol_command::sleep_mode:
		m_disabled = true;
		return true;
	case protocol_command::checksum_request:
		return true;
	case protocol_command::learn_escrow:
		m_jam_latched = false;
		m_reject_route = false;
		m_escrow_cents = 0;
		status_out.fault = 0x00U;
		status_out.sensor = 0x00U;
		return true;
	case protocol_command::enable_acceptance:
		m_disabled = false;
		return true;
	case protocol_command::collect_escrow:
		if (m_escrow_cents <= 0)
			return false;
		m_escrow_cents = 0;
		status_out.sensor = 0x00U;
		return true;
	case protocol_command::refund_escrow:
		if (m_escrow_cents <= 0)
			return false;
		m_escrow_cents = 0;
		m_reject_route = true;
		status_out.sensor = 0x00U;
		return true;
	case protocol_command::poll_status:
		return true;
	case protocol_command::reset_fault:
		m_jam_latched = false;
		m_reject_route = false;
		status_out.fault = 0x00U;
		return true;
	}
	return false;
}

void millennium_coin_model::protocol_note_timeout() noexcept
{
	m_protocol_timeout_count++;
	m_protocol_reset_pulse_count++;
	m_jam_latched = true;
}
