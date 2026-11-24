// SPDX-License-Identifier: GPL-2.0-or-later
// Track-2 decode vectors for sentinels, PAN extraction, and LRC classification.

#include "millennium_card_model.h"

#include <iostream>
#include <string>

int main()
{
	using model = millennium_card_model;

	{
		// Compose a valid body and append XOR LRC.
		std::string body = ";1234567890123456=25122010000012345678?";
		body.push_back(static_cast<char>(model::xor_lrc_byte(body)));
		auto const r = model::decode_track2_ascii(body, true);
		if (r.error != model::track2_decode_error::none || r.pan != "1234567890123456") {
			std::cerr << "valid track2 decode failed\n";
			return 1;
		}
	}

	{
		auto const r = model::decode_track2_ascii("1234567890123456=2512?", false);
		if (r.error != model::track2_decode_error::missing_start_sentinel) {
			std::cerr << "missing start sentinel not detected\n";
			return 2;
		}
	}

	{
		auto const r = model::decode_track2_ascii(";1234=2501", false);
		if (r.error != model::track2_decode_error::missing_end_sentinel) {
			std::cerr << "missing end sentinel not detected\n";
			return 3;
		}
	}

	{
		auto const r = model::decode_track2_ascii(";12A4=2501?", false);
		if (r.error != model::track2_decode_error::illegal_character) {
			std::cerr << "illegal character not detected\n";
			return 4;
		}
	}

	{
		std::string bad_lrc = ";123456=2501?";
		bad_lrc.push_back(static_cast<char>(0x7f));
		auto const r = model::decode_track2_ascii(bad_lrc, true);
		if (r.error != model::track2_decode_error::lrc_failed) {
			std::cerr << "lrc failure not detected\n";
			return 5;
		}
	}

	{
		std::string body = ";123456=2501?";
		body.push_back(static_cast<char>(model::xor_lrc_byte(body)));
		auto const r = model::decode_track2_ascii(body, true, true);
		if (r.error != model::track2_decode_error::parity_failed) {
			std::cerr << "parity failure not detected\n";
			return 6;
		}
	}

	{
		model reader;
		std::string error;
		if (!reader.parse_fixture_json(R"({"track":2,"payload_body":";12=34?"})", error)) {
			std::cerr << "Track 2 fixture rejected: " << error << '\n';
			return 7;
		}
		auto const bits = reader.track2_wire_bits();
		if (bits.size() != 64U || bits[12] != 1U || bits[13] != 1U || bits[14] != 0U ||
			bits[15] != 1U || bits[16] != 0U) {
			std::cerr << "magnetic start sentinel does not use five bits, least significant first\n";
			return 8;
		}
		unsigned lrc = 0;
		for (std::size_t offset = 12; offset < bits.size() - 12; offset += 5) {
			unsigned nibble = 0;
			unsigned parity = bits[offset + 4];
			for (unsigned i = 0; i < 4; ++i) {
				nibble |= bits[offset + i] << i;
				parity ^= bits[offset + i];
			}
			if (parity != 1) {
				std::cerr << "magnetic symbol has even parity\n";
				return 9;
			}
			lrc ^= nibble;
		}
		if (lrc != 0) {
			std::cerr << "magnetic longitudinal checksum does not match\n";
			return 10;
		}
	}

	std::cout << "card_track2_decode_vectors ok\n";
	return 0;
}
