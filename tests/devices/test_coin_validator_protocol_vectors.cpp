// SPDX-License-Identifier: GPL-2.0-or-later
// Coin validator protocol vectors: command handling and status framing.

#include "millennium_coin_model.h"

#include <iostream>

int main()
{
	millennium_coin_model m;
	millennium_coin_board_config cfg;
	cfg.validator_type = "uart";
	cfg.denominations_cents = {5, 10, 25};
	m.configure(cfg);
	m.reset();

	{
		// Insert creates escrow for collect/refund commands.
		if (!m.begin_insert_cents(25, 1000U, 12288000U) || !m.escrow_present()) {
			std::cerr << "failed to establish escrow state\n";
			return 1;
		}
		millennium_coin_model::protocol_status_frame st{};
		if (!m.protocol_handle_command(millennium_coin_model::protocol_command::poll_status, st)) {
			std::cerr << "poll_status command rejected\n";
			return 2;
		}
		if (st.code != 0x42U || st.sensor == 0U) {
			std::cerr << "status frame does not report escrow sensor\n";
			return 3;
		}
		if (!m.protocol_handle_command(millennium_coin_model::protocol_command::collect_escrow, st) || m.escrow_present()) {
			std::cerr << "collect_escrow did not clear escrow\n";
			return 4;
		}
	}

	{
		if (!m.begin_insert_cents(10, 2000U, 12288000U) || !m.escrow_present()) {
			std::cerr << "failed to establish escrow state for refund\n";
			return 5;
		}
		millennium_coin_model::protocol_status_frame st{};
		if (!m.protocol_handle_command(millennium_coin_model::protocol_command::refund_escrow, st) || m.escrow_present()) {
			std::cerr << "refund_escrow did not clear escrow\n";
			return 6;
		}
	}

	{
		m.inject_jam(3000U);
		millennium_coin_model::protocol_status_frame st{};
		if (!m.protocol_handle_command(millennium_coin_model::protocol_command::poll_status, st) || st.fault == 0U) {
			std::cerr << "jam fault not surfaced in status frame\n";
			return 7;
		}
		if (!m.protocol_handle_command(millennium_coin_model::protocol_command::reset_fault, st) || m.jammed()) {
			std::cerr << "reset_fault did not clear jam\n";
			return 8;
		}
	}

	{
		unsigned const timeouts_before = m.protocol_timeout_count();
		unsigned const resets_before = m.protocol_reset_pulse_count();
		m.protocol_note_timeout();
		if (m.protocol_timeout_count() != timeouts_before + 1U || m.protocol_reset_pulse_count() != resets_before + 1U) {
			std::cerr << "timeout accounting did not increment\n";
			return 9;
		}
	}

	{
		m.inject_jam(4000U);
		millennium_coin_model::protocol_status_frame st{};
		if (!m.protocol_handle_command(millennium_coin_model::protocol_command::learn_escrow, st) || m.jammed()) {
			std::cerr << "learn_escrow did not clear jam\n";
			return 10;
		}
		if (!m.protocol_handle_command(millennium_coin_model::protocol_command::wake, st)) {
			std::cerr << "wake rejected\n";
			return 11;
		}
	}

	m.reset();
	if (m.uart_command('g') != std::vector<std::uint8_t>{'A'} ||
		m.uart_command('r') != std::vector<std::uint8_t>{'B', 0, 0} || m.uart_insert(5)) {
		std::cerr << "validator wake, poll, or idle acceptance failed\n";
		return 12;
	}
	m.uart_command('d');
	m.uart_command('2');
	m.uart_command('x');
	if (m.uart_insert(10) || m.uart_insert(5) != '1' || m.uart_insert(25) || !m.escrow_present()) {
		std::cerr << "validator did not respect disabled window or one-coin mode\n";
		return 13;
	}
	m.uart_command('e');
	m.uart_command('2');
	m.uart_command('a');
	if (m.uart_insert(10) != '2' || m.uart_insert(25) != '3') {
		std::cerr << "validator reported incorrect slot numbers\n";
		return 14;
	}
	m.uart_command('f', 100);
	if (m.escrow_present()) {
		std::cerr << "validator refund left coins in escrow\n";
		return 15;
	}
	if (m.uart_command('r', 1100) != std::vector<std::uint8_t>{'B', 0, 0x08} ||
		m.uart_command('r', 2100) != std::vector<std::uint8_t>{'B', 0, 0}) {
		std::cerr << "validator refund position sensor did not show travel and completion\n";
		return 16;
	}

	std::cout << "coin_validator_protocol_vectors ok\n";
	return 0;
}
