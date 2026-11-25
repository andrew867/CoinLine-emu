// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_smartcard_model.h"

#include <algorithm>
#include <array>
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
	std::size_t p = colon + 1;
	while (p < json.size() && std::isspace(static_cast<unsigned char>(json[p])))
		++p;
	auto const token = [&](char const *value, std::size_t length) {
		if (json.compare(p, length, value) != 0)
			return false;
		std::size_t end = p + length;
		while (end < json.size() && std::isspace(static_cast<unsigned char>(json[end])))
			++end;
		return end == json.size() || json[end] == ',' || json[end] == '}';
	};
	if (token("true", 4)) {
		out = true;
		return true;
	}
	if (token("false", 5)) {
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
	if (m_fixture.protocol == "async") {
		(void)parse_json_uint(json_text, "\"inter_character_delay_us\"", m_fixture.inter_character_delay_us);
		(void)parse_json_string(json_text, "\"application\"", m_fixture.async_application);
		std::string reset = "external";
		(void)parse_json_string(json_text, "\"reset_policy\"", reset);
		if ((reset != "external" && reset != "internal") || ad < 112U || ad > 11175U ||
			apdu > 1000000U || m_fixture.inter_character_delay_us > 100000U ||
			(m_fixture.async_application != "none" && m_fixture.async_application != "lab-readonly") ||
			m_fixture.memory.size() > 4096U) {
			error_out = "invalid asynchronous laboratory fixture";
			return false;
		}
		m_fixture.async_internal_reset = reset == "internal";
	}
	bool auth = false;
	if (parse_json_bool(json_text, "\"requires_authorization\"", auth))
		m_fixture.requires_authorization = auth;
	(void)parse_json_bool(json_text, "\"sync_program_enabled\"", m_fixture.sync_program_enabled);
	if (m_fixture.sync_program_enabled) {
		if (m_fixture.protocol != "sync" ||
			!parse_json_uint(json_text, "\"sync_program_first_bit\"", m_fixture.sync_program_first_bit) ||
			!parse_json_uint(json_text, "\"sync_program_end_bit\"", m_fixture.sync_program_end_bit) ||
			m_fixture.sync_program_first_bit >= m_fixture.sync_program_end_bit ||
			m_fixture.sync_program_end_bit > m_fixture.memory.size() * 8U) {
			error_out = "invalid synchronous programming bounds";
			return false;
		}
		(void)parse_json_uint(json_text, "\"sync_program_us\"", m_fixture.sync_program_us);
		if (!m_fixture.sync_program_us || m_fixture.sync_program_us > 1000000U) {
			error_out = "invalid synchronous programming delay";
			return false;
		}
	}

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
	m_earlier_power_clock = false;
	clear_sync_session();
	clear_async_session();
}

void millennium_smartcard_model::insert_card_at(std::uint64_t cycle, std::uint64_t cpu_hz)
{
	clear_sync_session();
	clear_async_session();
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
	clear_sync_session();
	clear_async_session();
}

void millennium_smartcard_model::clear_sync_session()
{
	m_sync_selected = false;
	m_sync_powered = false;
	m_sync_reset = false;
	m_sync_clock = false;
	m_sync_released = true;
	m_sync_reset_clocked = false;
	m_sync_ready = false;
	m_sync_bit_address = 0;
	m_earlier_power_latch = false;
	m_later_power_latch = false;
	m_later_power_clock = false;
	m_sync_program_armed = false;
	m_sync_program_active = false;
	m_sync_program_start = 0;
}

void millennium_smartcard_model::drive_sync_pins(bool selected, bool power, bool reset,
	bool clock, bool released, std::uint64_t cycle)
{
	bool const clock_rise = clock && !m_sync_clock;
	bool const reset_rise = reset && !m_sync_reset;
	bool const reset_fall = !reset && m_sync_reset;
	m_sync_selected = selected;
	m_sync_released = released;
	if (!selected || !power || !m_card_present) {
		m_sync_powered = false;
		m_sync_ready = false;
		m_sync_reset_clocked = false;
		m_sync_bit_address = 0;
		m_sync_program_armed = false;
		m_sync_program_active = false;
	} else {
		if (!m_sync_powered) {
			m_sync_ready = false;
			m_sync_reset_clocked = false;
			m_sync_bit_address = 0;
		}
		m_sync_powered = true;
		if (reset_rise) {
			// A reset with a clock edge seeks to zero. A pulse without a
			// clock edge is a distinct programming request on writable fixtures.
			finish_sync_program(cycle);
			m_sync_program_armed = false;
			m_sync_reset_clocked = false;
		}
		if (reset && clock_rise)
			m_sync_reset_clocked = true;
		if (reset_fall && m_sync_reset_clocked) {
			m_sync_ready = true;
			m_sync_bit_address = 0;
			m_sync_program_armed = false;
		} else if (reset_fall && m_sync_ready && m_fixture.sync_program_enabled) {
			m_sync_program_armed = true;
			if (clock) {
				m_sync_program_start = cycle;
				m_sync_program_active = true;
				m_sync_program_armed = false;
			}
		} else if (!reset && clock_rise && m_sync_ready) {
			if (m_sync_program_armed) {
				m_sync_program_start = cycle;
				m_sync_program_active = true;
				m_sync_program_armed = false;
			} else if (m_sync_bit_address < m_fixture.memory.size() * 8U)
				++m_sync_bit_address;
		}
		if (!clock && m_sync_clock)
			finish_sync_program(cycle);
	}
	m_sync_reset = reset;
	m_sync_clock = clock;
}

void millennium_smartcard_model::finish_sync_program(std::uint64_t cycle)
{
	if (!m_sync_program_active)
		return;
	m_sync_program_active = false;
	std::uint64_t const minimum = (std::uint64_t(m_fixture.sync_program_us) * m_cpu_hz + 999999U) / 1000000U;
	if (cycle < m_sync_program_start || cycle - m_sync_program_start < minimum ||
		m_sync_bit_address < m_fixture.sync_program_first_bit ||
		m_sync_bit_address >= m_fixture.sync_program_end_bit ||
		(m_fixture.requires_authorization && !m_authorized))
		return;
	// The synthetic memory card permits only one-way programming. It cannot
	// restore an erased bit or modify data outside the explicit lab range.
	m_fixture.memory[m_sync_bit_address / 8U] &= std::uint8_t(~(0x80U >> (m_sync_bit_address % 8U)));
}

void millennium_smartcard_model::update_later_pins(std::uint8_t d, std::uint8_t e, std::uint8_t f, std::uint64_t cycle)
{
	bool const request = (e & 0x08U) != 0U;
	bool const power_clock = (f & 0x02U) != 0U;
	if (!request)
		m_later_power_latch = false;
	else if (power_clock && !m_later_power_clock && m_card_present)
		m_later_power_latch = true;
	m_later_power_clock = power_clock;
	// The manual synchronous clock/data contacts do not use the asynchronous
	// socket address. Both address values 0 and 3 occur in real contact tests.
	drive_sync_pins(request && !(e & 0x01U), m_later_power_latch,
		(e & 0x04U) != 0U, (d & 0x08U) != 0U, (d & 0x04U) != 0U, cycle);
}

void millennium_smartcard_model::update_earlier_pins(std::uint8_t a, std::uint8_t c, std::uint64_t cycle)
{
	bool const power_clock = (a & 0x40U) != 0U;
	// The earlier card interface latches supply on the enable pulse. A
	// falling edge immediately after that pulse leaves the contact powered.
	// Removal and terminal reset clear the selected contact session.
	if (!m_card_present)
		m_earlier_power_latch = false;
	else if (power_clock && !m_earlier_power_clock)
		m_earlier_power_latch = true;
	m_earlier_power_clock = power_clock;
	// The bidirectional contact has a pull-up while the low nibble of port C
	// is configured as input. The earlier board inverts the clock and reset
	// drive lines between the PIO latch and the card contacts.
	drive_sync_pins(m_earlier_power_latch, m_earlier_power_latch,
		(c & 0x80U) == 0U, (c & 0x40U) == 0U, true, cycle);
}

bool millennium_smartcard_model::sync_data_high() const noexcept
{
	if (!m_sync_released)
		return false;
	if (!m_sync_powered || !m_sync_ready || m_sync_reset || m_fixture.protocol != "sync")
		return true; // undriven contact
	if (m_sync_bit_address >= m_fixture.memory.size() * 8U)
		return true;
	return (m_fixture.memory[m_sync_bit_address / 8U] &
		(0x80U >> (m_sync_bit_address % 8U))) != 0U;
}

bool millennium_smartcard_model::restore_sync_test_memory(std::vector<std::uint8_t> const &memory)
{
	if (!sync_test_memory_writable() || memory.size() != m_fixture.memory.size())
		return false;
	// A persisted synthetic contact card may contain only changes possible
	// through the explicitly bounded one-way programming operation.
	for (std::size_t bit = 0; bit < memory.size() * 8U; ++bit) {
		std::uint8_t const mask = 0x80U >> (bit % 8U);
		bool const original = (m_fixture.memory[bit / 8U] & mask) != 0;
		bool const saved = (memory[bit / 8U] & mask) != 0;
		if (original != saved && (saved || bit < m_fixture.sync_program_first_bit || bit >= m_fixture.sync_program_end_bit))
			return false;
	}
	m_fixture.memory = memory;
	return true;
}

void millennium_smartcard_model::clear_async_session()
{
	m_selected_protocol = negotiated_protocol::none;
	m_authorized = false;
	m_async_routed = false;
	m_async_active = false;
	m_async_reset = false;
	m_async_selected_application = false;
	m_async_tx.clear();
	m_async_ready_cycle = 0;
	m_async_command.clear();
	m_pps_request.clear();
	m_pending_parameters.reset();
	m_pps_possible = true;
	m_t0_incoming_length = 0;
	m_t1_chain.clear();
	m_t1_last_request.clear();
	m_t1_last_reply.clear();
	m_t1_outgoing.clear();
	m_t1_ifsd = 32;
	m_t1_rx_sequence = false;
	m_t1_tx_sequence = false;
}

void millennium_smartcard_model::queue_async(std::vector<std::uint8_t> const &bytes, std::uint64_t cycle, unsigned delay_us)
{
	if (!m_async_active || bytes.empty())
		return;
	if (m_async_tx.empty())
		m_async_ready_cycle = cycle + (std::uint64_t(delay_us) * m_cpu_hz + 999999U) / 1000000U;
	m_async_tx.insert(m_async_tx.end(), bytes.begin(), bytes.end());
}

void millennium_smartcard_model::activate_async(std::uint64_t cycle)
{
	m_async_active = true;
	m_selected_protocol = negotiated_protocol::none;
	(void)negotiate_protocol(false);
	// The actual receiver must see even a malformed ATR to reject it. Only
	// a validated ATR enables subsequent command processing.
	queue_async(m_fixture.atr, cycle, m_fixture.atr_delay_us);
}

void millennium_smartcard_model::update_async_later_pins(std::uint8_t h, std::uint8_t d, std::uint8_t e, std::uint64_t cycle)
{
	if (m_fixture.protocol != "async")
		return;
	bool const routed = m_card_present && !(h & 0x08U) &&
		(d & 0x03U) == 3U && (e & 0x01U) && (e & 0x08U) && m_later_power_latch;
	bool const reset = (e & 0x04U) != 0U;
	bool const was_routed = m_async_routed;
	bool const was_reset = m_async_reset;
	if (!routed || (was_routed && was_reset && !reset))
		clear_async_session();
	m_async_routed = routed;
	m_async_reset = reset;
	if (!routed || m_async_active)
		return;
	if ((m_fixture.async_internal_reset && !was_routed && !reset) ||
		(!m_fixture.async_internal_reset && was_routed && !was_reset && reset))
		activate_async(cycle);
}

std::optional<std::uint8_t> millennium_smartcard_model::async_read_byte(std::uint64_t cycle)
{
	if (!m_async_routed || !m_async_active || m_async_tx.empty() || cycle < m_async_ready_cycle)
		return std::nullopt;
	std::uint8_t const byte = m_async_tx.front();
	m_async_tx.pop_front();
	return byte;
}

void millennium_smartcard_model::async_write_byte(std::uint8_t data, std::uint64_t cycle)
{
	if (!m_async_routed || !m_async_active || !m_async_tx.empty())
		return;
	if (m_selected_protocol == negotiated_protocol::none)
		return;
	if (m_pps_possible && (data == 0xffU || !m_pps_request.empty())) {
		async_pps_byte(data, cycle);
		return;
	}
	m_pps_possible = false;
	if (m_selected_protocol == negotiated_protocol::t0)
		async_t0_byte(data, cycle);
	else if (m_selected_protocol == negotiated_protocol::t1)
		async_t1_byte(data, cycle);
}

void millennium_smartcard_model::async_pps_byte(std::uint8_t data, std::uint64_t cycle)
{
	m_pps_request.push_back(data);
	if (m_pps_request.size() < 2U)
		return;
	std::uint8_t const pps0 = m_pps_request[1];
	unsigned const length = 3U + unsigned(bool(pps0 & 0x10U)) + unsigned(bool(pps0 & 0x20U)) + unsigned(bool(pps0 & 0x40U));
	if (m_pps_request.size() < length)
		return;
	std::uint8_t check = 0;
	for (auto byte : m_pps_request)
		check ^= byte;
	unsigned const selected = m_selected_protocol == negotiated_protocol::t1 ? 1U : 0U;
	bool valid = !check && !(pps0 & 0xe0U) && (pps0 & 0x0fU) == selected;
	if (valid && (pps0 & 0x10U)) {
		std::uint8_t const advertised = (m_fixture.atr[1] & 0x10U) ? m_fixture.atr[2] : 0x11U;
		std::uint8_t const parameter = m_pps_request[2];
		// Only explicit laboratory rates, never a guessed application speed.
		if (parameter != advertised && parameter != 0x11U)
			valid = false;
		else if (parameter == 0x11U)
			m_pending_parameters = serial_parameters{372, 1};
		else if (parameter == 0x12U)
			m_pending_parameters = serial_parameters{372, 2};
		else if (parameter == 0x94U || parameter == 0x95U)
			m_pending_parameters = serial_parameters{512, parameter == 0x94U ? 8U : 16U};
		else
			valid = false;
	}
	if (valid)
		queue_async(m_pps_request, cycle, m_fixture.apdu_response_delay_us);
	else
		m_pending_parameters.reset();
	m_pps_request.clear();
	m_pps_possible = false;
}

std::optional<millennium_smartcard_model::serial_parameters> millennium_smartcard_model::async_take_parameters()
{
	if (!m_async_active || !m_async_tx.empty())
		return std::nullopt;
	auto const parameters = m_pending_parameters;
	m_pending_parameters.reset();
	return parameters;
}

std::vector<std::uint8_t> millennium_smartcard_model::application_apdu(std::vector<std::uint8_t> const &apdu)
{
	static constexpr std::array<std::uint8_t, 5> lab_aid{0xf0, 0x4c, 0x41, 0x42, 0x01};
	if (apdu.size() < 5U)
		return {0x67, 0x00};
	if (apdu[0] != 0)
		return {0x6e, 0x00};
	if (apdu[1] == 0xa4U) {
		m_async_selected_application = false;
		if ((apdu[2] != 4U && apdu[2] != 0U) || apdu[3] != 0U)
			return {0x6a, 0x86};
		std::size_t const length = apdu[2] == 4U ? lab_aid.size() : 2U;
		if (apdu[4] != length || apdu.size() != 5U + length)
			return {0x67, 0x00};
		bool const known = apdu[2] == 4U ? std::equal(lab_aid.begin(), lab_aid.end(), apdu.begin() + 5) :
			(apdu[5] == 0x70U && apdu[6] == 0x01U);
		if (m_fixture.async_application != "lab-readonly" || !known)
			return {0x6a, 0x82};
		m_async_selected_application = true;
		return {0x90, 0x00};
	}
	if (apdu[1] == 0xb0U) {
		if (apdu.size() != 5U)
			return {0x67, 0x00};
		if (!m_async_selected_application || (m_fixture.requires_authorization && !m_authorized))
			return {0x69, 0x85};
		std::size_t const offset = (std::size_t(apdu[2]) << 8) | apdu[3];
		std::size_t const size = apdu[4] ? apdu[4] : 256U;
		if (offset > m_fixture.memory.size() || size > m_fixture.memory.size() - offset)
			return {0x6b, 0x00};
		std::vector<std::uint8_t> result(m_fixture.memory.begin() + offset, m_fixture.memory.begin() + offset + size);
		result.insert(result.end(), {0x90, 0x00});
		return result;
	}
	return {0x6d, 0x00};
}

void millennium_smartcard_model::async_t0_byte(std::uint8_t data, std::uint64_t cycle)
{
	m_async_command.push_back(data);
	if (m_async_command.size() < 5U)
		return;
	if (m_async_command.size() == 5U && m_async_command[0] == 0 && m_async_command[1] == 0xa4U &&
		(m_async_command[2] == 4U || m_async_command[2] == 0U) && m_async_command[3] == 0U && m_async_command[4] != 0U) {
		m_t0_incoming_length = m_async_command[4];
		queue_async({0xa4}, cycle, m_fixture.apdu_response_delay_us);
		return;
	}
	if (m_async_command.size() < 5U + m_t0_incoming_length)
		return;
	auto reply = application_apdu(m_async_command);
	// T=0 requires the procedure character before transferring response data.
	if (reply.size() > 2U)
		reply.insert(reply.begin(), m_async_command[1]);
	queue_async(reply, cycle, m_fixture.apdu_response_delay_us);
	m_async_command.clear();
	m_t0_incoming_length = 0;
}

std::vector<std::uint8_t> millennium_smartcard_model::t1_block(std::uint8_t pcb, std::vector<std::uint8_t> const &inf) const
{
	std::vector<std::uint8_t> block{0, pcb, std::uint8_t(inf.size())};
	block.insert(block.end(), inf.begin(), inf.end());
	std::uint8_t lrc = 0;
	for (auto byte : block)
		lrc ^= byte;
	block.push_back(lrc);
	return block;
}

std::vector<std::uint8_t> millennium_smartcard_model::t1_next_response()
{
	std::size_t const count = std::min<std::size_t>(m_t1_ifsd, m_t1_outgoing.size());
	std::uint8_t const pcb = (m_t1_tx_sequence ? 0x40U : 0U) | (count < m_t1_outgoing.size() ? 0x20U : 0U);
	std::vector<std::uint8_t> const inf(m_t1_outgoing.begin(), m_t1_outgoing.begin() + count);
	m_t1_outgoing.erase(m_t1_outgoing.begin(), m_t1_outgoing.begin() + count);
	m_t1_tx_sequence = !m_t1_tx_sequence;
	return t1_block(pcb, inf);
}

void millennium_smartcard_model::async_t1_byte(std::uint8_t data, std::uint64_t cycle)
{
	m_async_command.push_back(data);
	if (m_async_command.size() < 3U || m_async_command.size() < 4U + m_async_command[2])
		return;
	auto request = std::move(m_async_command);
	m_async_command.clear();
	std::uint8_t lrc = 0;
	for (auto byte : request)
		lrc ^= byte;
	auto const r_block = [&](std::uint8_t error) {
		return t1_block(std::uint8_t(0x80U | (m_t1_rx_sequence ? 0x10U : 0U) | error), {});
	};
	std::vector<std::uint8_t> reply;
	std::vector<std::uint8_t> const inf(request.begin() + 3, request.end() - 1);
	std::uint8_t const pcb = request[1];
	if (request[0] != 0 || lrc || request[2] == 255U)
		reply = r_block(lrc ? 1 : 2);
	else if (!(pcb & 0x80U)) {
		bool const sequence = (pcb & 0x40U) != 0U;
		if (sequence != m_t1_rx_sequence) {
			reply = request == m_t1_last_request && !m_t1_last_reply.empty() ? m_t1_last_reply : r_block(2);
		} else if ((pcb & 0x1fU) || inf.size() > 32U || !m_t1_outgoing.empty() || m_t1_chain.size() + inf.size() > 4096U) {
			reply = r_block(2);
		} else {
			m_t1_rx_sequence = !m_t1_rx_sequence;
			m_t1_chain.insert(m_t1_chain.end(), inf.begin(), inf.end());
			if (pcb & 0x20U)
				reply = r_block(0);
			else {
				m_t1_outgoing = application_apdu(m_t1_chain);
				m_t1_chain.clear();
				reply = t1_next_response();
			}
			m_t1_last_request = request;
			m_t1_last_reply = reply;
		}
	} else if ((pcb & 0xc0U) == 0x80U) {
		if (inf.empty() && !m_t1_last_reply.empty()) {
			if ((pcb & 0x0fU) || bool(pcb & 0x10U) != m_t1_tx_sequence)
				reply = m_t1_last_reply;
			else if (!m_t1_outgoing.empty()) {
				reply = t1_next_response();
				m_t1_last_reply = reply;
			}
		}
	} else if (pcb == 0xc0U && inf.empty()) {
		m_t1_rx_sequence = m_t1_tx_sequence = false;
		m_t1_chain.clear();
		m_t1_last_request.clear();
		m_t1_last_reply.clear();
		m_t1_outgoing.clear();
		reply = t1_block(0xe0, {});
	} else if (pcb == 0xc1U && inf.size() == 1U && inf[0] > 0U && inf[0] < 255U && m_t1_outgoing.empty()) {
		// The reader declares the largest card-to-reader information field.
		m_t1_ifsd = inf[0];
		reply = t1_block(0xe1, inf);
	} else if (pcb == 0xc2U && inf.empty()) {
		m_t1_chain.clear();
		m_t1_outgoing.clear();
		reply = t1_block(0xe2, {});
	} else
		reply = r_block(2);
	queue_async(reply, cycle, m_fixture.apdu_response_delay_us);
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
	auto const invalid = [&]() {
		m_selected_protocol = allow_sync_probe ? negotiated_protocol::sync_fallback : negotiated_protocol::none;
		return allow_sync_probe;
	};
	auto const &atr = m_fixture.atr;
	if (atr.size() < 2U || (atr[0] != 0x3bU && atr[0] != 0x3fU))
		return invalid();
	std::uint8_t const t0 = atr[1];
	std::size_t cursor = 2U;
	std::uint8_t y = static_cast<std::uint8_t>(t0 >> 4);
	bool tck = false;
	int first_protocol = -1;
	while (true) {
		for (unsigned mask = 1U; mask <= 4U; mask <<= 1U) {
			if ((y & mask) && ++cursor > atr.size())
				return invalid();
		}
		if (y & 0x8U) {
			if (cursor >= atr.size())
				return invalid();
			std::uint8_t const tdi = atr[cursor++];
			std::uint8_t const proto = static_cast<std::uint8_t>(tdi & 0x0fU);
			tck |= proto != 0U;
			if (first_protocol < 0 && proto != 15U)
				first_protocol = proto;
			y = static_cast<std::uint8_t>(tdi >> 4);
			continue;
		}
		break;
	}
	if (cursor + (t0 & 0x0fU) + unsigned(tck) != atr.size())
		return invalid();
	if (tck) {
		std::uint8_t checksum = 0;
		for (std::size_t i = 1; i < atr.size(); ++i)
			checksum ^= atr[i];
		if (checksum)
			return invalid();
	}
	if (first_protocol > 1)
		return invalid();
	m_selected_protocol = first_protocol == 1 ? negotiated_protocol::t1 : negotiated_protocol::t0;
	return true;
}
