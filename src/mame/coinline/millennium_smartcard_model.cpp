// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_smartcard_model.h"

#include <cctype>
#include <sstream>

namespace {

bool parse_hex_byte_pair(std::string const &tok, std::uint8_t &out)
{
	if (tok.size() != 2)
		return false;
	auto hv = [](char c) -> int {
		if (c >= '0' && c <= '9')
			return c - '0';
		if (c >= 'A' && c <= 'F')
			return 10 + (c - 'A');
		if (c >= 'a' && c <= 'f')
			return 10 + (c - 'a');
		return -1;
	};
	int const h = hv(tok[0]);
	int const l = hv(tok[1]);
	if (h < 0 || l < 0)
		return false;
	out = static_cast<std::uint8_t>((h << 4) | l);
	return true;
}

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

bool parse_atr_hex(std::string const &s, std::vector<std::uint8_t> &out)
{
	out.clear();
	std::string tok;
	for (char c : s) {
		if (std::isspace(static_cast<unsigned char>(c))) {
			if (tok.size() == 2) {
				std::uint8_t b = 0;
				if (!parse_hex_byte_pair(tok, b))
					return false;
				out.push_back(b);
			}
			tok.clear();
			continue;
		}
		tok.push_back(c);
		if (tok.size() == 2) {
			std::uint8_t b = 0;
			if (!parse_hex_byte_pair(tok, b))
				return false;
			out.push_back(b);
			tok.clear();
		}
	}
	if (!tok.empty())
		return false;
	return !out.empty();
}

bool parse_memory_hex(std::string const &s, std::vector<std::uint8_t> &out)
{
	out.clear();
	for (std::size_t i = 0; i < s.size();) {
		if (std::isspace(static_cast<unsigned char>(s[i]))) {
			++i;
			continue;
		}
		if (i + 1 >= s.size())
			return false;
		std::string const pair = s.substr(i, 2);
		std::uint8_t b = 0;
		if (!parse_hex_byte_pair(pair, b))
			return false;
		out.push_back(b);
		i += 2;
	}
	return true;
}

} // namespace

bool millennium_smartcard_model::parse_fixture_json(std::string const &json_text, std::string &error_out)
{
	m_fixture = millennium_smart_fixture{};
	std::string proto = "memory";
	(void)parse_json_string(json_text, "\"protocol\"", proto);
	m_fixture.protocol = proto;

	std::string atrs;
	if (!parse_json_string(json_text, "\"atr_hex\"", atrs)) {
		error_out = "missing atr_hex";
		return false;
	}
	if (!parse_atr_hex(atrs, m_fixture.atr)) {
		error_out = "bad atr_hex";
		return false;
	}

	std::string mem;
	if (parse_json_string(json_text, "\"memory_hex\"", mem)) {
		if (!mem.empty() && !parse_memory_hex(mem, m_fixture.memory)) {
			error_out = "bad memory_hex";
			return false;
		}
	}

	unsigned ad = 4000;
	(void)parse_json_uint(json_text, "\"atr_delay_us\"", ad);
	m_fixture.atr_delay_us = ad;
	unsigned apdu = 1000;
	(void)parse_json_uint(json_text, "\"apdu_response_delay_us\"", apdu);
	m_fixture.apdu_response_delay_us = apdu;
	bool auth = false;
	if (parse_json_bool(json_text, "\"requires_authorization\"", auth))
		m_fixture.requires_authorization = auth;

	return true;
}

void millennium_smartcard_model::reset_session()
{
	m_rx.clear();
	m_atr_pending.clear();
	m_card_present = false;
	m_reset_pending = false;
	m_atr_ready_cycle = 0;
	m_block_until_cycle = 0;
	m_atr_enqueued = false;
	m_read_addr = 0;
	m_cmd_phase = false;
	m_cmd0 = 0;
	m_fixture = millennium_smart_fixture{};
	m_selected_protocol = negotiated_protocol::none;
	m_authorized = false;
}

void millennium_smartcard_model::insert_card_at(std::uint64_t cycle, std::uint64_t cpu_hz)
{
	m_cpu_hz = cpu_hz ? cpu_hz : 12288000;
	m_card_present = true;
	m_rx.clear();
	m_atr_pending.clear();
	m_reset_pending = false;
	m_atr_ready_cycle = 0;
	m_atr_enqueued = false;
	m_block_until_cycle = cycle + (std::uint64_t(m_fixture.atr_delay_us) * (m_cpu_hz / 1000000ULL));
	if (m_block_until_cycle < cycle)
		m_block_until_cycle = cycle;
}

void millennium_smartcard_model::remove_card()
{
	m_card_present = false;
	m_rx.clear();
	m_atr_pending.clear();
	m_atr_enqueued = false;
	m_selected_protocol = negotiated_protocol::none;
	m_authorized = false;
}

void millennium_smartcard_model::notify_reset(std::uint64_t cycle)
{
	if (!m_card_present)
		return;
	m_reset_pending = true;
	m_rx.clear();
	m_atr_enqueued = false;
	m_block_until_cycle = cycle + (std::uint64_t(m_fixture.atr_delay_us) * (m_cpu_hz / 1000000ULL));
	m_cmd_phase = false;
}

std::uint8_t millennium_smartcard_model::status_lines() const
{
	std::uint8_t s = 0;
	if (m_card_present)
		s |= 0x01U;
	if (m_atr_enqueued || !m_rx.empty())
		s |= 0x02U;
	if (!m_rx.empty())
		s |= 0x04U;
	return s;
}

std::uint8_t millennium_smartcard_model::read_fifo(std::uint64_t cycle)
{
	if (!m_card_present)
		return 0xff;
	if (cycle < m_block_until_cycle)
		return 0xff;
	if (!m_atr_enqueued) {
		for (auto b : m_fixture.atr)
			m_rx.push_back(b);
		m_atr_enqueued = true;
	}
	if (!m_rx.empty()) {
		std::uint8_t const b = m_rx.front();
		m_rx.pop_front();
		return b;
	}
	return 0xff;
}

void millennium_smartcard_model::write_command(std::uint8_t b, std::uint64_t cycle)
{
	(void)cycle;
	if (m_fixture.requires_authorization && !m_authorized)
		return;
	if (!m_card_present || m_fixture.protocol != "memory")
		return;
	if (!m_cmd_phase) {
		m_cmd0 = b;
		m_cmd_phase = true;
		return;
	}
	if ((m_cmd0 & 0xf0U) == 0xb0U) {
		m_read_addr = unsigned(b) % (m_fixture.memory.empty() ? 1U : unsigned(m_fixture.memory.size()));
		if (!m_fixture.memory.empty())
			m_rx.push_back(m_fixture.memory[m_read_addr]);
	}
	m_cmd_phase = false;
}

bool millennium_smartcard_model::negotiate_protocol(bool allow_sync_probe)
{
	if (!m_card_present)
		return false;
	if (m_fixture.protocol == "memory") {
		m_selected_protocol = negotiated_protocol::t0;
		return true;
	}
	if (m_fixture.protocol == "sync") {
		m_selected_protocol = negotiated_protocol::sync_fallback;
		return true;
	}
	if (m_fixture.atr.size() < 2U) {
		if (allow_sync_probe) {
			m_selected_protocol = negotiated_protocol::sync_fallback;
			return true;
		}
		return false;
	}
	std::uint8_t const ts = m_fixture.atr[0];
	if (ts != 0x3bU && ts != 0x3fU) {
		if (allow_sync_probe) {
			m_selected_protocol = negotiated_protocol::sync_fallback;
			return true;
		}
		return false;
	}
	std::uint8_t const t0 = m_fixture.atr[1];
	std::size_t cursor = 2U;
	std::uint8_t y = static_cast<std::uint8_t>(t0 >> 4);
	bool seen_t0 = true;
	bool seen_t1 = false;
	while (true) {
		if (y & 0x1U)
			cursor++;
		if (y & 0x2U)
			cursor++;
		if (y & 0x4U)
			cursor++;
		if (y & 0x8U) {
			if (cursor >= m_fixture.atr.size())
				break;
			std::uint8_t const tdi = m_fixture.atr[cursor++];
			std::uint8_t const proto = static_cast<std::uint8_t>(tdi & 0x0fU);
			if (proto == 0x00U)
				seen_t0 = true;
			else if (proto == 0x01U)
				seen_t1 = true;
			y = static_cast<std::uint8_t>(tdi >> 4);
			continue;
		}
		break;
	}
	if (seen_t1)
		m_selected_protocol = negotiated_protocol::t1;
	else if (seen_t0)
		m_selected_protocol = negotiated_protocol::t0;
	else if (allow_sync_probe)
		m_selected_protocol = negotiated_protocol::sync_fallback;
	else
		return false;
	return true;
}
