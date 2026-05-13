// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_modem_ncc.h"

#include <cstdint>
#include <iostream>
#include <vector>

int main()
{
	using namespace coinline::modem::ncc;

	ncc_frame_fields tx{};
	tx.control = 0x02U; // packet_id=2
	tx.term_id = {0x10U, 0x20U, 0x30U, 0x40U, 0x50U};
	tx.payload = {0xAAU, 0xBBU, 0xCCU};

	std::vector<std::uint8_t> wire = encode(tx);
	if (wire.empty()) {
		std::cerr << "encode failed\n";
		return 1;
	}
	if (wire.front() != 0x02U || wire.back() != 0x03U) {
		std::cerr << "stx/etx mismatch\n";
		return 2;
	}
	// count includes control + term_id(5) + payload + crc16(2)
	std::uint8_t const expected_count = static_cast<std::uint8_t>(1U + 5U + tx.payload.size() + 2U);
	if (wire[2] != expected_count) {
		std::cerr << "count mismatch\n";
		return 3;
	}

	ncc_frame_fields rx{};
	std::string err;
	if (!decode(wire, rx, err)) {
		std::cerr << "decode failed: " << err << "\n";
		return 4;
	}
	if (rx.control != tx.control || rx.term_id != tx.term_id || rx.payload != tx.payload) {
		std::cerr << "roundtrip mismatch\n";
		return 5;
	}
	if (control_packet_id(rx.control) != 2U) {
		std::cerr << "packet id decode mismatch\n";
		return 6;
	}

	std::vector<std::uint8_t> bad_crc = wire;
	bad_crc[bad_crc.size() - 3U] ^= 0x01U;
	if (decode(bad_crc, rx, err)) {
		std::cerr << "bad crc accepted\n";
		return 7;
	}

	std::vector<std::uint8_t> bad_count = wire;
	bad_count[2] = static_cast<std::uint8_t>(bad_count[2] + 1U);
	if (decode(bad_count, rx, err)) {
		std::cerr << "bad count accepted\n";
		return 8;
	}

	std::vector<std::uint8_t> big_payload(246U, 0x5AU);
	tx.payload = big_payload;
	if (!encode(tx).empty()) {
		std::cerr << "oversized payload accepted\n";
		return 9;
	}

	std::cout << "modem_ncc_framing ok\n";
	return 0;
}

