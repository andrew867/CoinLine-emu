// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_card_model.h"

#include <cctype>
#include <sstream>

namespace {

bool parse_json_string(std::string const &json, char const *key, std::string &out)
{
	auto const kpos = json.find(key);
	if (kpos == std::string::npos)
		return false;
	auto const colon = json.find(':', kpos);
	if (colon == std::string::npos)
		return false;
	auto q1 = json.find('"', colon);
	if (q1 == std::string::npos)
		return false;
	auto q2 = json.find('"', q1 + 1);
	if (q2 == std::string::npos || q2 <= q1)
		return false;
	out = json.substr(q1 + 1, q2 - q1 - 1);
	return true;
}

bool parse_json_uint(std::string const &json, char const *key, unsigned &out)
{
	auto const kpos = json.find(key);
	if (kpos == std::string::npos)
		return false;
	auto const colon = json.find(':', kpos);
	if (colon == std::string::npos)
		return false;
	std::size_t i = colon + 1;
	while (i < json.size() && std::isspace(static_cast<unsigned char>(json[i])))
		++i;
	unsigned v = 0;
	if (i >= json.size() || !std::isdigit(static_cast<unsigned char>(json[i])))
		return false;
	while (i < json.size() && std::isdigit(static_cast<unsigned char>(json[i]))) {
		v = v * 10U + unsigned(json[i] - '0');
		++i;
	}
	out = v;
	return true;
}

bool parse_json_bool(std::string const &json, char const *key, bool &out)
{
	auto const kpos = json.find(key);
	if (kpos == std::string::npos)
		return false;
	auto const colon = json.find(':', kpos);
	if (colon == std::string::npos)
		return false;
	auto p = json.find("true", colon);
	if (p != std::string::npos && p < colon + 20) {
		out = true;
		return true;
	}
	p = json.find("false", colon);
	if (p != std::string::npos && p < colon + 22) {
		out = false;
		return true;
	}
	return false;
}

} // namespace

std::uint8_t millennium_card_model::xor_lrc_byte(std::string_view body_ascii)
{
	std::uint8_t x = 0;
	for (char c : body_ascii)
		x = static_cast<std::uint8_t>(x ^ static_cast<unsigned char>(c));
	return x;
}

bool millennium_card_model::track_xor_ok(std::string_view full_track_ascii)
{
	std::uint8_t x = 0;
	for (char c : full_track_ascii)
		x = static_cast<std::uint8_t>(x ^ static_cast<unsigned char>(c));
	return x == 0;
}

millennium_card_model::track2_decode_result millennium_card_model::decode_track2_ascii(std::string_view full_track_ascii,
	bool require_lrc, bool force_parity_error)
{
	track2_decode_result out{};
	if (full_track_ascii.empty()) {
		out.error = track2_decode_error::missing_start_sentinel;
		return out;
	}
	if (require_lrc && !track_xor_ok(full_track_ascii)) {
		out.error = track2_decode_error::lrc_failed;
		return out;
	}
	std::string_view body = full_track_ascii;
	if (require_lrc && body.size() > 1U)
		body = body.substr(0, body.size() - 1U);

	std::size_t start = body.find(';');
	std::size_t end = body.find('?');
	if (start == std::string_view::npos) {
		// Reverse swipe can present an inverted sentinel ordering. Mark and report.
		if (body.find('?') != std::string_view::npos)
			out.reverse_swipe = true;
		out.error = track2_decode_error::missing_start_sentinel;
		return out;
	}
	if (end == std::string_view::npos || end <= start) {
		out.error = track2_decode_error::missing_end_sentinel;
		return out;
	}
	std::string_view payload = body.substr(start + 1U, end - start - 1U);
	std::size_t const sep = payload.find('=');
	std::string_view pan = (sep == std::string_view::npos) ? payload : payload.substr(0, sep);
	if (pan.empty()) {
		out.error = track2_decode_error::empty_pan;
		return out;
	}
	for (char c : payload) {
		if ((c >= '0' && c <= '9') || c == '=') {
			continue;
		}
		out.error = track2_decode_error::illegal_character;
		return out;
	}
	if (force_parity_error) {
		out.error = track2_decode_error::parity_failed;
		return out;
	}
	out.pan.assign(pan.begin(), pan.end());
	out.body.assign(payload.begin(), payload.end());
	return out;
}

bool millennium_card_model::parse_fixture_json(std::string const &json_text, std::string &error_out)
{
	m_fix = millennium_mag_fixture{};
	std::string body;
	if (!parse_json_string(json_text, "\"payload_body\"", body)) {
		error_out = "missing payload_body";
		return false;
	}
	m_fix.payload_track_ascii = body;
	unsigned t = 2;
	if (parse_json_uint(json_text, "\"track\"", t))
		m_fix.track = t;
	unsigned dur = 350;
	(void)parse_json_uint(json_text, "\"swipe_duration_ms\"", dur);
	m_fix.swipe_duration_ms = dur;
	unsigned bps = 210;
	(void)parse_json_uint(json_text, "\"bit_rate_bps\"", bps);
	if (bps == 0) {
		error_out = "bit_rate_bps must be non-zero";
		return false;
	}
	m_fix.bit_rate_bps = bps;
	bool ferr = false;
	if (parse_json_bool(json_text, "\"force_lrc_error\"", ferr))
		m_fix.force_lrc_error = ferr;
	bool perr = false;
	if (parse_json_bool(json_text, "\"force_parity_error\"", perr))
		m_fix.force_parity_error = perr;

	std::uint8_t const lrc = xor_lrc_byte(m_fix.payload_track_ascii);
	char const lc = static_cast<char>(lrc);
	if (m_fix.force_lrc_error)
		m_fix.payload_track_ascii.push_back(static_cast<char>(lrc ^ 0x5a));
	else
		m_fix.payload_track_ascii.push_back(lc);

	m_lrc_ok = track_xor_ok(m_fix.payload_track_ascii);
	rebuild_bits_from_track();
	return true;
}

void millennium_card_model::rebuild_bits_from_track()
{
	m_bits.clear();
	for (unsigned char const uc : m_fix.payload_track_ascii) {
		for (int b = 0; b < 8; ++b)
			m_bits.push_back(static_cast<std::uint8_t>((uc >> b) & 1));
	}
}

void millennium_card_model::reset_session()
{
	m_armed = false;
	m_swipe_start_cycle = 0;
	m_bits.clear();
	m_lrc_ok = false;
	m_fix = millennium_mag_fixture{};
	m_cpu_hz_arm = 1;
	m_sensor_cp_stuck = false;
	m_sensor_cfs_stuck = false;
}

void millennium_card_model::set_path_sensors_stuck(bool cp_stuck, bool cfs_stuck) noexcept
{
	m_sensor_cp_stuck = cp_stuck;
	m_sensor_cfs_stuck = cfs_stuck;
}

void millennium_card_model::arm_swipe(std::uint64_t start_cycle, std::uint64_t cpu_hz)
{
	m_swipe_start_cycle = start_cycle;
	m_armed = true;
	m_cpu_hz_arm = cpu_hz ? cpu_hz : 1;
}

void millennium_card_model::abort_swipe()
{
	m_armed = false;
}

std::uint64_t millennium_card_model::cycles_per_bit(std::uint64_t cpu_hz) const
{
	if (!m_fix.bit_rate_bps)
		return 1;
	return cpu_hz / std::uint64_t(m_fix.bit_rate_bps);
}

bool millennium_card_model::swipe_active(std::uint64_t cycle) const
{
	if (!m_armed || m_bits.empty())
		return false;
	std::uint64_t const cpb = cycles_per_bit(m_cpu_hz_arm);
	std::uint64_t const last_bit_cycle = m_swipe_start_cycle + cpb * std::uint64_t(m_bits.empty() ? 0 : m_bits.size() - 1);
	std::uint64_t const window_end = m_swipe_start_cycle + std::uint64_t(m_fix.swipe_duration_ms) * (m_cpu_hz_arm / 1000);
	std::uint64_t const end = last_bit_cycle > window_end ? last_bit_cycle : window_end;
	return cycle <= end && cycle >= m_swipe_start_cycle;
}

std::uint8_t millennium_card_model::status_bits(std::uint64_t cycle) const
{
	std::uint8_t s = 0;
	if (!m_fix.payload_track_ascii.empty())
		s |= 0x01U;
	if (m_sensor_cp_stuck)
		s |= 0x20U;
	if (m_sensor_cfs_stuck)
		s |= 0x40U;
	if (!m_lrc_ok)
		s |= 0x08U;
	if (m_armed && !m_bits.empty()) {
		std::uint64_t const cpb = cycles_per_bit(m_cpu_hz_arm);
		std::uint64_t const idx = (cycle > m_swipe_start_cycle) ? (cycle - m_swipe_start_cycle) / cpb : 0;
		if (idx < m_bits.size()) {
			s |= 0x02U;
			s |= 0x04U;
		} else if (cycle >= m_swipe_start_cycle && !m_bits.empty()) {
			s |= 0x10U;
		}
	}
	return s;
}

std::uint8_t millennium_card_model::data_byte(std::uint64_t cycle) const
{
	if (!m_armed || m_bits.empty())
		return 0xff;
	std::uint64_t const cpb = cycles_per_bit(m_cpu_hz_arm);
	if (cpb == 0)
		return 0xfe;
	std::uint64_t const idx = (cycle > m_swipe_start_cycle) ? (cycle - m_swipe_start_cycle) / cpb : 0;
	if (idx >= m_bits.size())
		return 0xff;
	return static_cast<std::uint8_t>(m_bits[static_cast<std::size_t>(idx)] & 1);
}
