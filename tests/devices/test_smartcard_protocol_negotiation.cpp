// SPDX-License-Identifier: GPL-2.0-or-later
// Smartcard ATR negotiation and synchronous fallback vectors.

#include "millennium_smartcard_model.h"

#include <iostream>

int main()
{
	{
		millennium_smartcard_model m;
		std::string err;
		// T=0-only ATR.
		if (!m.parse_fixture_json("{\"protocol\":\"memory\",\"atr_hex\":\"3B 00\",\"memory_hex\":\"00\"}", err)) {
			std::cerr << "parse failed for t0 fixture\n";
			return 1;
		}
		m.insert_card_at(0U, 12288000U);
		if (!m.negotiate_protocol(false) || m.selected_protocol() != millennium_smartcard_model::negotiated_protocol::t0) {
			std::cerr << "t0 negotiation failed\n";
			return 2;
		}
	}

	{
		millennium_smartcard_model m;
		std::string err;
		// ATR with TD1 low nibble = 1 => T=1 available.
		if (!m.parse_fixture_json("{\"protocol\":\"micro\",\"atr_hex\":\"3B 90 11 01\"}", err)) {
			std::cerr << "parse failed for t1 fixture\n";
			return 3;
		}
		m.insert_card_at(0U, 12288000U);
		if (!m.negotiate_protocol(false) || m.selected_protocol() != millennium_smartcard_model::negotiated_protocol::t1) {
			std::cerr << "t1 negotiation failed\n";
			return 4;
		}
	}

	{
		millennium_smartcard_model m;
		std::string err;
		if (!m.parse_fixture_json("{\"protocol\":\"micro\",\"atr_hex\":\"00 90 11 00\"}", err)) {
			std::cerr << "parse failed for bad-ts fixture\n";
			return 5;
		}
		m.insert_card_at(0U, 12288000U);
		if (m.negotiate_protocol(false)) {
			std::cerr << "negotiation should fail without sync fallback\n";
			return 6;
		}
		if (!m.negotiate_protocol(true)
			|| m.selected_protocol() != millennium_smartcard_model::negotiated_protocol::sync_fallback) {
			std::cerr << "sync fallback negotiation failed\n";
			return 7;
		}
	}

	{
		millennium_smartcard_model m;
		std::string err;
		if (!m.parse_fixture_json(
				"{\"protocol\":\"memory\",\"atr_hex\":\"3B 00\",\"memory_hex\":\"01020304\",\"requires_authorization\":true}",
				err)) {
			std::cerr << "parse failed for auth fixture\n";
			return 8;
		}
		m.insert_card_at(0U, 12288000U);
		(void)m.read_fifo(100000U);
		(void)m.read_fifo(100000U);
		m.write_command(0xb0, 100000U);
		m.write_command(0x01, 100000U);
		if (m.read_fifo(101000U) != 0xffU) {
			std::cerr << "unauthorized read should not return data\n";
			return 9;
		}
		m.authorize_session(true);
		m.write_command(0xb0, 102000U);
		m.write_command(0x01, 102000U);
		if (m.read_fifo(103000U) != 0x02U) {
			std::cerr << "authorized memory read mismatch\n";
			return 10;
		}
	}

	std::cout << "smartcard_protocol_negotiation ok\n";
	return 0;
}
