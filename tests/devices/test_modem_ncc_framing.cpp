// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_modem_ncc.h"

#include <cstdint>
#include <iostream>
#include <vector>

int main()
{
	using namespace coinline::modem::ncc;
	// Independent CRC-16/ARC check value, plus literal NCC wire vectors.
	constexpr std::array<std::uint8_t, 9> check{'1', '2', '3', '4', '5', '6', '7', '8', '9'};
	if (compute_crc16(check.data(), check.size()) != 0xbb3dU || compute_crc16(nullptr, 0U) != 0U) {
		std::cerr << "CRC check vector mismatch\n";
		return 1;
	}

	ncc_frame_fields tx{};
	tx.control = 0x02U; // packet_id=2
	tx.term_id = {0x10U, 0x20U, 0x30U, 0x40U, 0x50U};
	tx.payload = {0xAAU, 0xBBU, 0xCCU};

	std::vector<std::uint8_t> wire = encode(tx);
	std::vector<std::uint8_t> const expected_wire = {
		0x02, 0x02, 0x0d, 0x10, 0x20, 0x30, 0x40, 0x50, 0xaa, 0xbb, 0xcc, 0x95, 0x96, 0x03
	};
	if (wire != expected_wire) {
		std::cerr << "message wire vector mismatch\n";
		return 1;
	}
	if (wire.front() != 0x02U || wire.back() != 0x03U) {
		std::cerr << "stx/etx mismatch\n";
		return 2;
	}
	// Wire length is count + 1; message count = 10 + data length.
	std::uint8_t const expected_count = static_cast<std::uint8_t>(10U + tx.payload.size());
	if (wire[2] != expected_count) {
		std::cerr << "count mismatch\n";
		return 3;
	}

	ncc_frame_fields rx{};
	std::string err;
	if (!decode(expected_wire, rx, err)) {
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
	tx.payload.resize(245);
	std::vector<std::uint8_t> const maximum = encode(tx);
	if (maximum.size() != 256U || maximum[2] != 255U || !decode(maximum, rx, err)
		|| rx.payload != tx.payload) {
		std::cerr << "maximum frame rejected\n";
		return 10;
	}
	tx.payload.clear();
	if (!encode(tx).empty()) {
		std::cerr << "empty message accepted\n";
		return 11;
	}
	// Observed terminal-clear wire frame: control packets carry no ID or data.
	std::vector<std::uint8_t> const clear = {0x02, 0x20, 0x05, 0x78, 0x03, 0x03};
	std::vector<std::uint8_t> const ack = {0x02, 0x08, 0x05, 0x66, 0x03, 0x03};
	for (auto const &literal : {clear, ack}) {
		if (!decode(literal, rx, err) || rx.control != literal[1] || !rx.payload.empty()
			|| rx.term_id != std::array<std::uint8_t, 5>{} || encode(rx) != literal) {
			std::cerr << "control frame vector mismatch\n";
			return 12;
		}
	}
	// Geometry and flag validation, even with a valid CRC over the malformed bytes.
	for (std::uint8_t const invalid : {std::uint8_t(0x00), std::uint8_t(0x18), std::uint8_t(0xc8)}) {
		auto bad_control = ack;
		bad_control[1] = invalid;
		auto const crc = compute_crc16(bad_control.data(), 3U);
		bad_control[3] = static_cast<std::uint8_t>(crc);
		bad_control[4] = static_cast<std::uint8_t>(crc >> 8);
		if (decode(bad_control, rx, err)) {
			std::cerr << "malformed control accepted\n";
			return 13;
		}
	}
	auto concatenated = expected_wire;
	concatenated.insert(concatenated.end(), ack.begin(), ack.end());
	std::vector<std::uint8_t> const truncated(expected_wire.begin(), expected_wire.end() - 1);
	if (decode(concatenated, rx, err) || decode(truncated, rx, err)) {
		std::cerr << "non-single-frame input accepted\n";
		return 14;
	}

	std::cout << "modem_ncc_framing ok\n";
	return 0;
}

