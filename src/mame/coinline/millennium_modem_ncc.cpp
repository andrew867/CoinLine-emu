// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_modem_ncc.h"

namespace coinline::modem::ncc {

namespace {
constexpr std::uint8_t k_stx = 0x02U;
constexpr std::uint8_t k_etx = 0x03U;
constexpr std::size_t k_term_id_bytes = 5U;
constexpr std::size_t k_max_packet_size = 256U;
constexpr std::size_t k_max_payload_size = 245U;
constexpr std::uint8_t k_control_count = 5U;
constexpr std::uint8_t k_message_overhead = k_control_count + k_term_id_bytes;

bool valid_control(std::uint8_t control) noexcept
{
	return !(control & CONTROL_SPARE_MASK)
		&& (control & (CONTROL_ACK | CONTROL_NACK)) != (CONTROL_ACK | CONTROL_NACK);
}
} // namespace

std::uint16_t compute_crc16(std::uint8_t const *bytes, std::size_t length) noexcept
{
	// Reflected polynomial 0xA001, initial value zero, no final XOR.
	std::uint16_t crc = 0U;
	for (std::size_t i = 0; i < length; ++i) {
		crc ^= bytes[i];
		for (unsigned bit = 0; bit < 8U; ++bit)
			crc = (crc & 1U) ? static_cast<std::uint16_t>((crc >> 1) ^ 0xA001U) : static_cast<std::uint16_t>(crc >> 1);
	}
	return crc;
}

std::vector<std::uint8_t> encode(ncc_frame_fields const &f)
{
	std::vector<std::uint8_t> out;
	bool const is_control = (f.control & CONTROL_MASK) != 0;
	if (!valid_control(f.control) || (is_control ? !f.payload.empty()
		: f.payload.empty() || f.payload.size() > k_max_payload_size))
		return out;
	std::uint8_t const count = is_control ? k_control_count
		: static_cast<std::uint8_t>(k_message_overhead + f.payload.size());
	out.reserve(static_cast<std::size_t>(count) + 1U);
	out.push_back(k_stx);
	out.push_back(f.control);
	out.push_back(count);
	if (!is_control) {
		out.insert(out.end(), f.term_id.begin(), f.term_id.end());
		out.insert(out.end(), f.payload.begin(), f.payload.end());
	}
	std::uint16_t const crc = compute_crc16(out.data(), out.size());
	out.push_back(static_cast<std::uint8_t>(crc & 0xffU));
	out.push_back(static_cast<std::uint8_t>((crc >> 8) & 0xffU));
	out.push_back(k_etx);
	return out;
}

bool decode(std::vector<std::uint8_t> const &wire, ncc_frame_fields &out, std::string &error)
{
	error.clear();
	if (wire.size() < k_control_count + 1U) {
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
	if (!valid_control(control)) {
		error = "invalid control";
		return false;
	}
	bool const is_control = (control & CONTROL_MASK) != 0;
	if (is_control ? count != k_control_count : count <= k_message_overhead) {
		error = "invalid count";
		return false;
	}
	// The wire length is count + 1, including STX and ETX.
	std::size_t const expected_total = static_cast<std::size_t>(count) + 1U;
	if (wire.size() != expected_total) {
		error = "count mismatch";
		return false;
	}
	std::size_t const crc_pos = wire.size() - 3U;
	std::uint8_t const crc_lo = wire[crc_pos];
	std::uint8_t const crc_hi = wire[crc_pos + 1U];
	std::uint16_t const got = static_cast<std::uint16_t>(crc_lo | (static_cast<std::uint16_t>(crc_hi) << 8));
	std::uint16_t const want = compute_crc16(wire.data(), crc_pos);
	if (got != want) {
		error = "crc mismatch";
		return false;
	}
	out.control = control;
	out.term_id.fill(0);
	out.payload.clear();
	if (!is_control) {
		for (std::size_t i = 0; i < k_term_id_bytes; ++i)
			out.term_id[i] = wire[3U + i];
		out.payload.assign(wire.begin() + 3U + k_term_id_bytes, wire.begin() + crc_pos);
	}
	return true;
}

void session_model::on_tx_frame(std::uint8_t control) noexcept
{
	last_frame_was_nack = false;
	local_packet_id = control_packet_id(control);
	if ((control & CONTROL_CLEAR_CALL) != 0U) {
		state = session_state::idle;
		retry_count = 0U;
		return;
	}
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
	if ((frame.control & CONTROL_CLEAR_CALL) != 0U) {
		state = session_state::idle;
		retry_count = 0U;
		last_frame_was_nack = false;
		return;
	}
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
