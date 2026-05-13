// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_modem_model.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>

namespace {

bool parse_hex_byte(char const *start, char const *end, unsigned &out)
{
	out = 0;
	unsigned digits = 0;
	for (char const *p = start; p < end; ++p) {
		unsigned v = 0;
		if (*p >= '0' && *p <= '9')
			v = unsigned(*p - '0');
		else if (*p >= 'a' && *p <= 'f')
			v = 10 + unsigned(*p - 'a');
		else if (*p >= 'A' && *p <= 'F')
			v = 10 + unsigned(*p - 'A');
		else if (std::isspace(static_cast<unsigned char>(*p)))
			continue;
		else
			return false;
		out = (out << 4) | v;
		++digits;
		if (digits > 2)
			return false;
	}
	return digits > 0;
}

bool parse_hex_line(std::string const &line, std::vector<std::uint8_t> &bytes, std::string &err)
{
	bytes.clear();
	std::istringstream iss(line);
	std::string tok;
	while (iss >> tok) {
		unsigned v = 0;
		if (!parse_hex_byte(tok.data(), tok.data() + tok.size(), v)) {
			err = "bad hex token: " + tok;
			return false;
		}
		bytes.push_back(static_cast<std::uint8_t>(v));
	}
	return true;
}

std::string trim_copy(std::string s)
{
	while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ' || s.back() == '\t'))
		s.pop_back();
	std::size_t i = 0;
	while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i])))
		++i;
	return s.substr(i);
}

} // namespace

char const *millennium_modem_model::state_cstr(millennium_modem_state s) noexcept
{
	switch (s) {
	case millennium_modem_state::idle:
		return "idle";
	case millennium_modem_state::dialing:
		return "dialing";
	case millennium_modem_state::ringing:
		return "ringing";
	case millennium_modem_state::connected:
		return "connected";
	case millennium_modem_state::busy:
		return "busy";
	case millennium_modem_state::no_answer:
		return "no_answer";
	case millennium_modem_state::carrier_lost:
		return "carrier_lost";
	case millennium_modem_state::noisy_line:
		return "noisy_line";
	default:
		return "?";
	}
}

void millennium_modem_model::declared_states(std::vector<millennium_modem_state> &out_states)
{
	out_states = {
		millennium_modem_state::idle,
		millennium_modem_state::dialing,
		millennium_modem_state::ringing,
		millennium_modem_state::connected,
		millennium_modem_state::busy,
		millennium_modem_state::no_answer,
		millennium_modem_state::carrier_lost,
		millennium_modem_state::noisy_line,
	};
}

void millennium_modem_model::reset()
{
	m_state = millennium_modem_state::idle;
	m_dcd = false;
	m_cts = true;
	m_rts = false;
	m_m8_pending = false;
	m_m8_latched = false;
	m_transcript.clear();
	m_clean_connect_line = 0;
	m_in_clean_connect_replay = false;
}

void millennium_modem_model::set_dtr(bool v) noexcept
{
	m_dtr = v;
	if (!v && m_state != millennium_modem_state::connected)
		goto_idle();
}

void millennium_modem_model::set_rts(bool v) noexcept
{
	m_rts = v;
}

void millennium_modem_model::goto_idle()
{
	m_state = millennium_modem_state::idle;
	m_dcd = false;
}

void millennium_modem_model::append_tx(std::uint8_t b)
{
	m_transcript.push_back({'t', b});
}

void millennium_modem_model::append_rx(std::uint8_t b)
{
	m_transcript.push_back({'r', b});
}

void millennium_modem_model::note_tx(std::uint8_t b)
{
	append_tx(b);
}

void millennium_modem_model::push_rx(std::uint8_t b)
{
	append_rx(b);
}

void millennium_modem_model::note_asci_programmed(std::uint8_t cntla, std::uint8_t cntlb) noexcept
{
	(void)cntla;
	(void)cntlb;
	if (!m_m8_latched) {
		m_m8_pending = true;
		m_m8_latched = true;
	}
}

bool millennium_modem_model::consume_m8_pending() noexcept
{
	bool const v = m_m8_pending;
	m_m8_pending = false;
	return v;
}

bool millennium_modem_model::advance_clean_connect_line(unsigned line_index)
{
	m_in_clean_connect_replay = true;
	if (line_index >= 3)
		return true;
	switch (line_index) {
	case 0:
		m_state = millennium_modem_state::dialing;
		break;
	case 1:
		m_state = millennium_modem_state::ringing;
		break;
	case 2:
		m_state = millennium_modem_state::connected;
		m_dcd = true;
		break;
	default:
		break;
	}
	return true;
}

bool millennium_modem_model::inject_event(char const *name)
{
	if (!name)
		return false;
	if (!std::strcmp(name, "carrier_lost")) {
		if (m_state == millennium_modem_state::connected && m_dcd) {
			m_state = millennium_modem_state::carrier_lost;
			m_dcd = false;
			return true;
		}
		m_state = millennium_modem_state::carrier_lost;
		m_dcd = false;
		return true;
	}
	if (!std::strcmp(name, "ringing")) {
		m_state = millennium_modem_state::ringing;
		return true;
	}
	if (!std::strcmp(name, "busy")) {
		m_state = millennium_modem_state::busy;
		return true;
	}
	if (!std::strcmp(name, "no_answer")) {
		m_state = millennium_modem_state::no_answer;
		return true;
	}
	if (!std::strcmp(name, "noisy_line")) {
		m_state = millennium_modem_state::noisy_line;
		return true;
	}
	if (!std::strcmp(name, "connected")) {
		m_state = millennium_modem_state::connected;
		m_dcd = true;
		return true;
	}
	if (!std::strcmp(name, "dialing")) {
		m_state = millennium_modem_state::dialing;
		return true;
	}
	if (!std::strcmp(name, "recover_idle")) {
		goto_idle();
		return true;
	}
	if (!std::strcmp(name, "idle")) {
		goto_idle();
		return true;
	}
	return false;
}

bool millennium_modem_model::replay_fixture_text(std::string const &text, std::string &error_out)
{
	error_out.clear();
	std::istringstream in(text);
	std::string line;
	unsigned hex_line_index = 0;
	while (std::getline(in, line)) {
		line = trim_copy(line);
		if (line.empty())
			continue;
		if (line[0] == '#') {
			std::string rest = trim_copy(line.substr(1));
			if (rest.rfind("event", 0) == 0) {
				rest = trim_copy(rest.substr(std::strlen("event")));
				if (!inject_event(rest.c_str())) {
					error_out = "unknown event: " + rest;
					return false;
				}
			}
			continue;
		}
		std::vector<std::uint8_t> bytes;
		if (!parse_hex_line(line, bytes, error_out))
			return false;
		for (std::uint8_t b : bytes)
			push_rx(b);
		advance_clean_connect_line(hex_line_index);
		++hex_line_index;
	}
	return true;
}
