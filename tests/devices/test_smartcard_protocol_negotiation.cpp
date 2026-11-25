// SPDX-License-Identifier: GPL-2.0-or-later
// Smartcard ATR negotiation and synchronous fallback vectors.

#include "millennium_smartcard_model.h"

#include <iostream>

int main()
{
	{
		millennium_smartcard_model m;
		std::string err;
		if (!m.parse_fixture_json(R"({"protocol":"sync","atr_hex":"00","memory_hex":"FFFF","sync_program_enabled":true,"sync_program_first_bit":1,"sync_program_end_bit":4,"sync_program_us":10})", err))
			return 30;
		m.insert_card_at(0, 1000000);
		m.update_earlier_pins(0x40, 0x00, 1);
		m.update_earlier_pins(0x40, 0x80, 2);
		m.update_earlier_pins(0x40, 0xc0, 3);
		m.update_earlier_pins(0x40, 0x80, 4);
		m.update_earlier_pins(0x40, 0x00, 5);
		// Bit zero is outside the permitted test range.
		m.update_earlier_pins(0x40, 0x80, 6);
		m.update_earlier_pins(0x40, 0x00, 7);
		m.update_earlier_pins(0x40, 0x40, 8);
		m.update_earlier_pins(0x40, 0x00, 20);
		if (m.memory()[0] != 0xff || m.sync_bit_address() != 0) return 31;
		m.update_earlier_pins(0x40, 0x40, 21);
		m.update_earlier_pins(0x40, 0x00, 22);
		// Short programming pulse must not modify memory or advance the address.
		m.update_earlier_pins(0x40, 0x80, 23);
		m.update_earlier_pins(0x40, 0x00, 24);
		m.update_earlier_pins(0x40, 0x40, 25);
		m.update_earlier_pins(0x40, 0x00, 34);
		if (m.memory()[0] != 0xff || m.sync_bit_address() != 1) return 32;
		m.update_earlier_pins(0x40, 0x80, 35);
		m.update_earlier_pins(0x40, 0x00, 36);
		m.update_earlier_pins(0x40, 0x40, 37);
		m.update_earlier_pins(0x40, 0x00, 47);
		if (m.memory()[0] != 0xbf || m.sync_data_high() || m.sync_bit_address() != 1) return 33;
		// Repeating a successful write cannot restore the bit.
		m.update_earlier_pins(0x40, 0x80, 48);
		m.update_earlier_pins(0x40, 0x00, 49);
		m.update_earlier_pins(0x40, 0x40, 50);
		m.update_earlier_pins(0x40, 0x00, 60);
		if (m.memory()[0] != 0xbf || m.memory()[1] != 0xff) return 34;
		m.update_earlier_pins(0x40, 0x40, 61);
		m.update_earlier_pins(0x40, 0x00, 62);
		m.update_earlier_pins(0x40, 0x80, 63);
		m.update_earlier_pins(0x40, 0x00, 64);
		m.update_earlier_pins(0x40, 0x40, 65);
		m.remove_card();
		m.insert_card_at(90, 1000000);
		if (m.memory()[0] != 0xbf || m.sync_powered()) return 35;
		if (m.parse_fixture_json(R"({"protocol":"sync","atr_hex":"00","memory_hex":"FF","sync_program_enabled":true,"sync_program_first_bit":0,"sync_program_end_bit":9})", err)) return 36;
	}
	{
		millennium_smartcard_model m;
		std::string err;
		if (!m.parse_fixture_json("{\"protocol\":\"sync\",\"atr_hex\":\"00\",\"memory_hex\":\"A55A\"}", err))
			return 20;
		m.insert_card_at(0, 12288000);
		m.update_later_pins(0x04, 0x08, 0x00); // another socket selected
		if (m.sync_data_selected()) return 28;
		// Later-board supply request alone does not power the contact.
		m.update_later_pins(0x07, 0x08, 0x00);
		if (m.sync_powered() || !m.sync_data_high()) return 21;
		m.update_later_pins(0x07, 0x08, 0x02);
		m.update_later_pins(0x07, 0x08, 0x00);
		if (!m.sync_powered() || !m.sync_data_high()) return 22;
		// Reset is qualified by a clock pulse and release, then bit zero is visible.
		m.update_later_pins(0x07, 0x0c, 0x00);
		m.update_later_pins(0x0f, 0x0c, 0x00);
		m.update_later_pins(0x07, 0x0c, 0x00);
		m.update_later_pins(0x07, 0x08, 0x00);
		unsigned value = 0;
		for (unsigned i = 0; i < 16; ++i) {
			if (m.sync_bit_address() != i) return 23;
			value = (value << 1) | unsigned(m.sync_data_high());
			m.update_later_pins(0x0f, 0x08, 0x00);
			m.update_later_pins(0x0f, 0x08, 0x00); // same level is not another edge
			m.update_later_pins(0x07, 0x08, 0x00);
		}
		if (value != 0xa55aU || !m.sync_data_high()) return 24;
		m.remove_card();
		m.update_later_pins(0x07, 0x08, 0x00);
		if (m.sync_powered() || !m.sync_data_high()) return 25;
		m.insert_card_at(0, 12288000);
		// Earlier-board contact uses direct supply and port C clock/reset.
		m.update_earlier_pins(0x40, 0x00);
		m.update_earlier_pins(0x40, 0x80);
		m.update_earlier_pins(0x40, 0xc0);
		m.update_earlier_pins(0x40, 0x80);
		m.update_earlier_pins(0x40, 0x00);
		value = 0;
		for (unsigned i = 0; i < 8; ++i) {
			value = (value << 1) | unsigned(m.sync_data_high());
			m.update_earlier_pins(0x40, 0x40);
			m.update_earlier_pins(0x40, 0x00);
		}
		if (value != 0xa5U) return 26;
		m.update_earlier_pins(0x00, 0x00);
		if (m.sync_powered() || m.sync_data_selected()) return 27;
	}
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
