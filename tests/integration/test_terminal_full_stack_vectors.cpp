// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_card_model.h"
#include "millennium_coin_model.h"
#include "millennium_modem_ncc.h"
#include "millennium_smartcard_model.h"
#include "millennium_terminal_peripherals_model.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

int main()
{
	using namespace coinline::modem::ncc;
	using namespace coinline::terminal;

	{
		ncc_frame_fields f{};
		f.control = 0x01U;
		f.term_id = std::array<std::uint8_t, 5>{0x01U, 0x02U, 0x03U, 0x04U, 0x05U};
		f.payload = std::vector<std::uint8_t>{0x2AU, 0x10U, 0x20U};
		std::vector<std::uint8_t> const w = encode(f);
		ncc_frame_fields d{};
		std::string err;
		if (w.empty() || !decode(w, d, err) || d.control != f.control || d.payload != f.payload) {
			std::cerr << "ncc integration vector failed\n";
			return 1;
		}
	}

	{
		millennium_coin_model coin;
		millennium_coin_board_config cfg{};
		cfg.denominations_cents = {5, 10, 25};
		coin.configure(cfg);
		coin.reset();
		if (!coin.begin_insert_cents(25, 100U, 12288000ULL)) {
			std::cerr << "coin begin insert failed\n";
			return 2;
		}
		coin.write_control(0x01U, 110U);
		if (!coin.disabled()) {
			std::cerr << "coin disable control not applied\n";
			return 3;
		}
	}

	{
		millennium_card_model card;
		std::string err;
		card.reset_session();
		char const *fixture =
			R"({"track":2,"payload_body":";1234567890123456=25122010000012345678?","swipe_duration_ms":350,"bit_rate_bps":210})";
		if (!card.parse_fixture_json(fixture, err)) {
			std::cerr << "card fixture parse failed: " << err << "\n";
			return 4;
		}
		card.arm_swipe(1000U, 12288000ULL);
		if (card.bit_count() == 0U) {
			std::cerr << "card swipe bitstream empty\n";
			return 5;
		}
	}

	{
		millennium_smartcard_model s;
		std::string err;
		char const *fx = R"({"protocol":"memory","atr_hex":"3B 95 11 00 FE 11","memory_hex":"11223344","atr_delay_us":1000})";
		if (!s.parse_fixture_json(fx, err)) {
			std::cerr << "smart fixture parse failed: " << err << "\n";
			return 6;
		}
		s.reset_session();
		s.insert_card_at(100U, 12288000ULL);
		s.notify_reset(120U);
		(void)s.read_fifo(100000U);
		if (s.protocol() != "memory") {
			std::cerr << "smart protocol mismatch\n";
			return 7;
		}
	}

	{
		peripheral_health_model h;
		h.request_owner(peripheral_health_model::owner_coin);
		h.force_disconnect_supervision_priority();
		if (h.owner() != peripheral_health_model::owner_disconnect_supervision) {
			std::cerr << "supervision ownership priority failed\n";
			return 8;
		}
		h.on_telephony_alive_result(false);
		h.on_telephony_alive_result(false);
		h.on_telephony_alive_result(false);
		if (!h.alarm_tel_not_responding()) {
			std::cerr << "telephony no-response threshold failed\n";
			return 9;
		}
	}

	std::cout << "terminal_full_stack_vectors ok\n";
	return 0;
}
