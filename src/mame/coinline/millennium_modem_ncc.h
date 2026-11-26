// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace coinline::modem::ncc {

struct ncc_frame_fields {
	std::uint8_t control = 0;
	std::array<std::uint8_t, 5> term_id{};
	std::vector<std::uint8_t> payload{};
};

// NCC CRC covers exactly STX through the last data byte, excluding CRC and ETX.
std::uint16_t compute_crc16(std::uint8_t const *bytes, std::size_t length) noexcept;

// Control packets have no terminal ID or payload on the wire. Message packets
// contain five ID bytes and 1..245 data bytes. An invalid frame encodes to empty.
std::vector<std::uint8_t> encode(ncc_frame_fields const &f);

// Strict single-frame decoder.
bool decode(std::vector<std::uint8_t> const &wire, ncc_frame_fields &out, std::string &error);

constexpr std::uint8_t CONTROL_PACKET_ID_MASK = 0x03U;
constexpr std::uint8_t CONTROL_RETRANSMIT = 0x04U;
constexpr std::uint8_t CONTROL_ACK = 0x08U;
constexpr std::uint8_t CONTROL_NACK = 0x10U;
constexpr std::uint8_t CONTROL_CLEAR_CALL = 0x20U;
constexpr std::uint8_t CONTROL_MASK = 0x38U;
constexpr std::uint8_t CONTROL_SPARE_MASK = 0xc0U;

inline std::uint8_t control_packet_id(std::uint8_t control) noexcept { return control & CONTROL_PACKET_ID_MASK; }

enum class session_state : std::uint8_t {
	idle = 0,
	waiting_ack,
	established,
	failed,
};

struct session_config {
	unsigned max_retries = 3U;
};

struct session_model {
	session_state state = session_state::idle;
	session_config config{};
	std::uint8_t local_packet_id = 0U;
	unsigned retry_count = 0U;
	bool last_frame_was_nack = false;

	// Called after transmitting a frame that requires ACK/NACK processing.
	void on_tx_frame(std::uint8_t control) noexcept;
	// Called when an inbound frame is decoded.
	void on_rx_frame(ncc_frame_fields const &frame) noexcept;
	// Called when the ACK wait window expires.
	void on_ack_timeout() noexcept;
};

} // namespace coinline::modem::ncc
