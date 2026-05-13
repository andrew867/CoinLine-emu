// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_modem_ncc.h"

namespace coinline::modem::ncc {

namespace {
constexpr std::uint8_t k_stx = 0x02U;
constexpr std::uint8_t k_etx = 0x03U;
constexpr std::size_t k_term_id_bytes = 5U;
constexpr std::size_t k_max_packet_size = 256U;
constexpr std::size_t k_max_payload_size = 245U;
} // namespace

std::uint16_t compute_crc16(std::uint8_t control, std::uint8_t count, std::array<std::uint8_t, 5> const &term_id,
	std::vector<std::uint8_t> const &payload) noexcept
{
	// Provisional NCC CRC16 implementation: CRC-16/IBM (poly 0xA001, init 0xFFFF), little-endian on wire.
	std::uint16_t crc = 0xFFFFU;
	auto const step = [&crc](std::uint8_t b) {
		crc ^= b;
		for (unsigned i = 0; i < 8U; ++i)
			crc = (crc & 1U) ? static_cast<std::uint16_t>((crc >> 1) ^ 0xA001U) : static_cast<std::uint16_t>(crc >> 1);
	};
	step(control);
	step(count);
	for (std::uint8_t b : term_id)
		step(b);
	for (std::uint8_t b : payload)
		step(b);
	return crc;
}

std::vector<std::uint8_t> encode(ncc_frame_fields const &f)
{
	std::vector<std::uint8_t> out;
	if (f.payload.size() > k_max_payload_size)
		return out;
	std::uint8_t const count = static_cast<std::uint8_t>(1U + k_term_id_bytes + f.payload.size() + 2U);
	std::uint16_t const crc = compute_crc16(f.control, count, f.term_id, f.payload);
	out.reserve(1U + 1U + 1U + k_term_id_bytes + f.payload.size() + 2U + 1U);
	out.push_back(k_stx);
	out.push_back(f.control);
	out.push_back(count);
	for (std::uint8_t b : f.term_id)
		out.push_back(b);
	for (std::uint8_t b : f.payload)
		out.push_back(b);
	out.push_back(static_cast<std::uint8_t>(crc & 0xffU));
	out.push_back(static_cast<std::uint8_t>((crc >> 8) & 0xffU));
	out.push_back(k_etx);
	if (out.size() > k_max_packet_size)
		out.clear();
	return out;
}

bool decode(std::vector<std::uint8_t> const &wire, ncc_frame_fields &out, std::string &error)
{
	error.clear();
	if (wire.size() < 1U + 1U + 1U + k_term_id_bytes + 2U + 1U) {
		error = "frame too short";
		return false;
	}
	if (wire.size() > k_max_packet_size) {
		error = "frame too large";
		return false;
	}
	if (wire.front() != k_stx) {
		error = "missing STX";
		return false;
	}
	if (wire.back() != k_etx) {
		error = "missing ETX";
		return false;
	}
	std::uint8_t const control = wire[1];
	std::uint8_t const count = wire[2];
	if (count < (1U + k_term_id_bytes + 2U)) {
		error = "invalid count";
		return false;
	}
	// [stx][control][count][term_id(5)][payload...][crc_lo][crc_hi][etx]
	std::size_t const expected_total = static_cast<std::size_t>(count) + 3U;
	if (wire.size() != expected_total) {
		error = "count mismatch";
		return false;
	}
	std::size_t const payload_len = static_cast<std::size_t>(count) - (1U + k_term_id_bytes + 2U);
	if (payload_len > k_max_payload_size) {
		error = "payload too large";
		return false;
	}
	std::array<std::uint8_t, 5> term_id{};
	for (std::size_t i = 0; i < k_term_id_bytes; ++i)
		term_id[i] = wire[3U + i];
	std::vector<std::uint8_t> payload;
	payload.reserve(payload_len);
	for (std::size_t i = 0; i < payload_len; ++i)
		payload.push_back(wire[3U + k_term_id_bytes + i]);
	std::uint8_t const crc_lo = wire[3U + k_term_id_bytes + payload_len];
	std::uint8_t const crc_hi = wire[3U + k_term_id_bytes + payload_len + 1U];
	std::uint16_t const got = static_cast<std::uint16_t>(crc_lo | (static_cast<std::uint16_t>(crc_hi) << 8));
	std::uint16_t const want = compute_crc16(control, count, term_id, payload);
	if (got != want) {
		error = "crc mismatch";
		return false;
	}
	out.control = control;
	out.term_id = term_id;
	out.payload = std::move(payload);
	return true;
}

void session_model::on_tx_frame(std::uint8_t control) noexcept
{
	last_frame_was_nack = false;
	local_packet_id = control_packet_id(control);
	if ((control & CONTROL_ACK) != 0U || (control & CONTROL_NACK) != 0U) {
		state = session_state::established;
		return;
	}
	if (state == session_state::failed)
		return;
	state = session_state::waiting_ack;
}

void session_model::on_rx_frame(ncc_frame_fields const &frame) noexcept
{
	std::uint8_t const remote_packet_id = control_packet_id(frame.control);
	if ((frame.control & CONTROL_NACK) != 0U && remote_packet_id == local_packet_id) {
		last_frame_was_nack = true;
		if (state != session_state::failed)
			state = session_state::waiting_ack;
		return;
	}
	if ((frame.control & CONTROL_ACK) != 0U && remote_packet_id == local_packet_id) {
		retry_count = 0U;
		last_frame_was_nack = false;
		if (state != session_state::failed)
			state = session_state::established;
		return;
	}
	if ((frame.control & CONTROL_CLEAR_CALL) != 0U) {
		state = session_state::idle;
		retry_count = 0U;
		last_frame_was_nack = false;
		return;
	}
}

void session_model::on_ack_timeout() noexcept
{
	if (state != session_state::waiting_ack || state == session_state::failed)
		return;
	retry_count++;
	if (retry_count >= config.max_retries)
		state = session_state::failed;
}

} // namespace coinline::modem::ncc
