// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_modem_model.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <sstream>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

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

millennium_modem_model::~millennium_modem_model()
{
	ncc_close();
}

void millennium_modem_model::reset()
{
	ncc_close();
	m_state = millennium_modem_state::idle;
	m_dcd = false;
	m_cts = true;
	m_rts = false;
	m_m8_pending = false;
	m_m8_latched = false;
	m_transcript.clear();
	m_clean_connect_line = 0;
	m_in_clean_connect_replay = false;
	m_dialed.clear();
	m_last_digit_sec = -1.0;
	m_ncc_up = false;
	m_ncc_attempted = false;
	m_ncc_retry_after = 0.0;
	m_ncc_rx.clear();
	m_ncc_lab_hello_sent = false;
	m_ncc_lab_hello_at = -1.0;
	m_ncc_tx_pending.clear();
	m_ssi_cr0 = m_ssi_cr1 = m_ssi_cr2 = m_ssi_cr3 = m_ssi_dr = m_ssi_tr = 0;
	m_ssi_pio = 0;
	m_ssi_shift = 0;
	m_ssi_bit = -1;
	m_ssi_reading = false;
	m_ssi_mpd = false;
	m_ssi_write_shift = 0;
	m_ssi_write_bits = 0;
	m_cp = cp_phase::idle;
	m_ncc_connected_at = -1.0;
	m_r1 = false;
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

void millennium_modem_model::ncc_send_bytes(std::uint8_t const *data, std::size_t n)
{
	if (m_ncc_up)
		m_ncc_tx_pending.insert(m_ncc_tx_pending.end(), data, data + n);
}

void millennium_modem_model::flush_ncc_tx()
{
	if (m_ncc_tx_pending.empty())
		return;
#if defined(_WIN32)
	int const sent = ::send(static_cast<SOCKET>(m_ncc_sock),
		reinterpret_cast<char const *>(m_ncc_tx_pending.data()), int(m_ncc_tx_pending.size()), 0);
#else
	ssize_t const sent = ::send(int(m_ncc_sock), m_ncc_tx_pending.data(), m_ncc_tx_pending.size(), MSG_NOSIGNAL);
#endif
	if (sent <= 0)
		return;
	m_ncc_tx_pending.erase(m_ncc_tx_pending.begin(), m_ncc_tx_pending.begin() + sent);
}

void millennium_modem_model::ncc_maybe_lab_hello()
{
	if (m_ncc_lab_hello_sent || !m_ncc_up)
		return;
	char const *env = std::getenv("COINLINE_NCC_LAB_HELLO");
	if (env == nullptr || env[0] != '1')
		return;
	static std::uint8_t const k_hello[] = {
		0x02, 0x00, 0x0D, 0x01, 0x02, 0x03, 0x04, 0x05, 0x07, 0x01, 0x02, 0x71, 0x8B, 0x03
	};
	ncc_send_bytes(k_hello, sizeof(k_hello));
	m_ncc_lab_hello_sent = true;
	m_ncc_lab_hello_at = -1.0;
	std::fprintf(stderr, "ncc lab hello %u bytes\n", unsigned(sizeof(k_hello)));
}

void millennium_modem_model::note_tx(std::uint8_t b)
{
	append_tx(b);
	if (char const *tr = std::getenv("COINLINE_MODEM_TRACE")) {
		if (tr[0] == '1')
			std::fprintf(stderr, "modem tx 0x%02X ncc_up=%d\n", unsigned(b), int(m_ncc_up));
	}
	if (m_dcd)
		ncc_send_bytes(&b, 1);
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
	if (!std::strcmp(name, "recover_idle") || !std::strcmp(name, "idle")) {
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

namespace {

void ncc_net_init()
{
#if defined(_WIN32)
	static bool once = false;
	if (!once) {
		WSADATA w{};
		(void)::WSAStartup(MAKEWORD(2, 2), &w);
		once = true;
	}
#endif
}

bool decimal_digits_to_ipv4(std::string const &digits, std::uint32_t &host_order)
{
	if (digits.empty() || digits.size() > 10)
		return false;
	unsigned long long n = 0;
	for (char c : digits) {
		if (c < '0' || c > '9')
			return false;
		n = n * 10ULL + unsigned(c - '0');
		if (n > 0xffffffffULL)
			return false;
	}
	if (n == 0)
		return false;
	host_order = static_cast<std::uint32_t>(n);
	return true;
}

unsigned ncc_port_from_env()
{
	unsigned port = 4000;
	if (char const *env = std::getenv("COINLINE_NCC_PORT")) {
		unsigned long const v = std::strtoul(env, nullptr, 10);
		if (v > 0 && v < 65536)
			port = unsigned(v);
	}
	return port;
}

} // namespace

void millennium_modem_model::ncc_close() noexcept
{
	m_ncc_up = false;
	m_ncc_tx_pending.clear();
	m_ncc_rx.clear();
	if (m_ncc_sock < 0)
		return;
#if defined(_WIN32)
	::closesocket(static_cast<SOCKET>(m_ncc_sock));
#else
	::close(int(m_ncc_sock));
#endif
	m_ncc_sock = -1;
}

void millennium_modem_model::refresh_detect_register()
{
	std::uint8_t dr = 0;
	switch (m_cp) {
	case cp_phase::dial_tone:
		dr = m_line_present ? 0x02 : 0x00;
		break;
	case cp_phase::ringback:
		dr = 0x02;
		break;
	case cp_phase::post_dial:
		dr = 0x00;
		break;
	case cp_phase::answer_tone:
		dr = 0x04;
		break;
	case cp_phase::carrier_mark:
		dr = 0x28;
		break;
	case cp_phase::carrier:
		dr = 0x08;
		break;
	case cp_phase::idle:
	default:
		dr = 0x00;
		break;
	}
	m_ssi_dr = dr;
}

void millennium_modem_model::poll_call_progress(double now_sec)
{
	if (!m_r1 || !m_ncc_up)
		return;
	double const elapsed = now_sec - m_ncc_connected_at;
	cp_phase next = cp_phase::ringback;
	if (elapsed >= 5.0)
		next = cp_phase::carrier;
	else if (elapsed >= 3.5)
		next = cp_phase::carrier_mark;
	else if (elapsed >= 2.0)
		next = cp_phase::answer_tone;
	if (next == m_cp)
		return;
	m_cp = next;
	refresh_detect_register();
	if (m_cp == cp_phase::carrier) {
		m_dcd = true;
		m_state = millennium_modem_state::connected;
		std::fprintf(stderr, "modem carrier (SSI) dcd=1\n");
	}
}

void millennium_modem_model::note_modem_relay(bool r1_active, double now_sec)
{
	(void)now_sec;
	if (r1_active == m_r1)
		return;
	m_r1 = r1_active;
	m_dialed.clear();
	m_last_digit_sec = -1.0;
	m_ncc_attempted = false;
	m_ncc_retry_after = 0.0;
	m_dcd = false;
	if (r1_active) {
		m_cp = cp_phase::dial_tone;
		m_state = millennium_modem_state::dialing;
		std::fprintf(stderr, "modem R1 seize\n");
	} else {
		ncc_close();
		m_cp = cp_phase::idle;
		m_state = millennium_modem_state::idle;
		std::fprintf(stderr, "modem R1 release\n");
	}
	refresh_detect_register();
}

char millennium_modem_model::ssi_pio_a_write(std::uint8_t pio_a, bool extended_address)
{
	// Modem PIO A uses active-low RD/WR and an LSB-first transfer.
	std::uint8_t const prev = m_ssi_pio;
	m_ssi_pio = pio_a;
	bool const rd_prev = (prev & 0x04U) != 0U;
	bool const rd_now = (pio_a & 0x04U) != 0U;
	bool const clk_prev = (prev & 0x02U) != 0U;
	bool const clk_now = (pio_a & 0x02U) != 0U;
	bool const wr_prev = (prev & 0x08U) != 0U;
	bool const wr_now = (pio_a & 0x08U) != 0U;
	unsigned const addr = (pio_a >> 4) & (extended_address ? 0x07U : 0x03U);

	auto reg_value = [&](unsigned a) -> std::uint8_t {
		switch (a) {
		case 0: return m_ssi_cr0;
		case 1: return m_ssi_cr1;
		case 2: return m_ssi_dr;
		case 3: return m_ssi_tr;
		case 4: return m_ssi_cr2;
		case 5: return m_ssi_cr3;
		default: return 0;
		}
	};
	auto store_reg = [&](unsigned a, std::uint8_t v) {
		switch (a) {
		case 0: m_ssi_cr0 = v; break;
		case 1: m_ssi_cr1 = v; break;
		case 2: break;
		case 3: m_ssi_tr = v; break;
		case 4: m_ssi_cr2 = v; break;
		case 5: m_ssi_cr3 = v; break;
		default: break;
		}
	};

	// RD/ falling edge presents bit 0. The following clocks advance the read.
	if (rd_prev && !rd_now) {
		refresh_detect_register();
		m_ssi_reading = true;
		m_ssi_write_bits = 0;
		m_ssi_write_shift = 0;
		m_ssi_shift = reg_value(addr);
		m_ssi_bit = 0;
		m_ssi_mpd = (m_ssi_shift & 1U) != 0U;
		return 0;
	}
	if (!rd_now && m_ssi_reading && clk_prev && !clk_now && m_ssi_bit < 7) {
		++m_ssi_bit;
		m_ssi_mpd = ((m_ssi_shift >> m_ssi_bit) & 1U) != 0U;
	}
	if (!rd_prev && rd_now) {
		m_ssi_reading = false;
		m_ssi_bit = -1;
	}

	// Clocks load the shift register while WR/ is inactive. The subsequent
	// low-to-high strobe latches all eight bits into the selected register.
	if (wr_now && rd_now && clk_prev && !clk_now && m_ssi_write_bits < 8) {
		if ((pio_a & 1U) == 0U)
			m_ssi_write_shift |= static_cast<std::uint8_t>(1U << m_ssi_write_bits);
		++m_ssi_write_bits;
	}
	if (wr_prev || !wr_now)
		return 0;

	auto const audible_digit = [this]() -> char {
		if ((m_ssi_tr & 0x10U) == 0 || (m_ssi_cr0 & 0x02U) == 0)
			return 0;
		return "D1234567890*#ABC"[m_ssi_tr & 0x0fU];
	};
	char const prior_digit = audible_digit();
	if (m_ssi_write_bits == 8)
		store_reg(addr, m_ssi_write_shift);
	m_ssi_write_bits = 0;
	m_ssi_write_shift = 0;
	char const digit = audible_digit();
	return m_r1 && digit != prior_digit ? digit : 0;
}

void millennium_modem_model::note_outpulse_digit(char digit, double now_sec)
{
	if (!m_r1 || !((digit >= '0' && digit <= '9') || digit == '#' || digit == '*'))
		return;
	m_last_digit_sec = now_sec;
	m_cp = cp_phase::post_dial;
	refresh_detect_register();
	if (m_state == millennium_modem_state::idle)
		m_state = millennium_modem_state::dialing;
	if (digit == '#') {
		if (!m_ncc_up) {
			m_ncc_attempted = false;
			m_ncc_retry_after = 0.0;
			ncc_connect_dialed(now_sec);
		}
		return;
	}
	if (digit == '*')
		return;
	if (m_dialed.size() < 16)
		m_dialed.push_back(digit);
	if (char const *path = std::getenv("COINLINE_DIALED_LOG")) {
		if (FILE *f = std::fopen(path, "w")) {
			std::fputs(m_dialed.c_str(), f);
			std::fputc('\n', f);
			std::fclose(f);
		}
	}
}

bool millennium_modem_model::ncc_connect_host(std::uint32_t host, unsigned port, double now_sec)
{
	if (m_ncc_up)
		return true;
	if (m_ncc_attempted && now_sec < m_ncc_retry_after)
		return false;
	ncc_net_init();
#if defined(_WIN32)
	SOCKET s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s == INVALID_SOCKET) {
		m_ncc_attempted = true;
		m_ncc_retry_after = now_sec + 1.0;
		return false;
	}
	u_long nb = 1;
	(void)::ioctlsocket(s, FIONBIO, &nb);
	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(static_cast<u_short>(port));
	addr.sin_addr.s_addr = htonl(host);
	int const cr = ::connect(s, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
	if (cr != 0 && ::WSAGetLastError() != WSAEWOULDBLOCK) {
		::closesocket(s);
		m_ncc_attempted = true;
		m_ncc_retry_after = now_sec + 1.0;
		return false;
	}
	fd_set wset;
	FD_ZERO(&wset);
	FD_SET(s, &wset);
	timeval tv{};
	tv.tv_sec = 0;
	tv.tv_usec = 400000;
	int const sel = ::select(0, nullptr, &wset, nullptr, &tv);
	int soerr = 0;
	int solen = int(sizeof(soerr));
	if (sel > 0)
		(void)::getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&soerr), &solen);
	if (sel <= 0 || soerr != 0) {
		::closesocket(s);
		m_ncc_attempted = true;
		m_ncc_retry_after = now_sec + 1.0;
		return false;
	}
	m_ncc_sock = static_cast<std::intptr_t>(s);
#else
	int s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s < 0)
		return false;
	int flags = ::fcntl(s, F_GETFL, 0);
	(void)::fcntl(s, F_SETFL, flags | O_NONBLOCK);
	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(static_cast<uint16_t>(port));
	addr.sin_addr.s_addr = htonl(host);
	int const cr = ::connect(s, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
	if (cr != 0 && errno != EINPROGRESS) {
		::close(s);
		return false;
	}
	m_ncc_sock = s;
#endif
	m_ncc_attempted = true;
	m_ncc_up = true;
	m_ncc_connected_at = now_sec;
	m_ncc_lab_hello_sent = false;
	m_ncc_lab_hello_at = now_sec + 0.4;
	m_ncc_tx_pending.clear();
	unsigned const a = (host >> 24) & 0xffU;
	unsigned const b = (host >> 16) & 0xffU;
	unsigned const c = (host >> 8) & 0xffU;
	unsigned const d = host & 0xffU;
	std::fprintf(stderr, "ncc connected %u.%u.%u.%u:%u (tcp; dcd deferred)\n", a, b, c, d, port);
	return true;
}

bool millennium_modem_model::ncc_connect_dialed(double now_sec)
{
	if (m_ncc_up || m_ncc_attempted)
		return m_ncc_up;
	if (now_sec < m_ncc_retry_after)
		return false;
	std::uint32_t host = 0;
	if (m_dialed.size() != 10 || !decimal_digits_to_ipv4(m_dialed, host) || host != 0x7f000001U)
		return false;
	bool const ok = ncc_connect_host(host, ncc_port_from_env(), now_sec);
	if (ok)
		std::fprintf(stderr, "ncc dialed=%s (dcd deferred to SSI carrier)\n", m_dialed.c_str());
	return ok;
}

void millennium_modem_model::poll_ncc_link(double now_sec)
{
	poll_call_progress(now_sec);
	if (m_r1 && !m_ncc_up && !m_ncc_attempted && m_last_digit_sec >= 0.0 && (now_sec - m_last_digit_sec) >= 1.2
		&& m_dialed.size() == 10)
		ncc_connect_dialed(now_sec);
	if (!m_ncc_up || m_ncc_sock < 0)
		return;
	if (!m_ncc_lab_hello_sent && m_ncc_lab_hello_at >= 0.0 && now_sec >= m_ncc_lab_hello_at)
		ncc_maybe_lab_hello();
	flush_ncc_tx();
	std::uint8_t buf[64];
#if defined(_WIN32)
	int const n = ::recv(static_cast<SOCKET>(m_ncc_sock), reinterpret_cast<char *>(buf), int(sizeof(buf)), 0);
#else
	ssize_t const n = ::recv(int(m_ncc_sock), buf, sizeof(buf), 0);
#endif
	if (n == 0) {
		ncc_close();
		m_dcd = false;
		m_cp = cp_phase::post_dial;
		refresh_detect_register();
		m_state = millennium_modem_state::carrier_lost;
	} else if (n > 0) {
		m_ncc_rx.insert(m_ncc_rx.end(), buf, buf + n);
	}
}

bool millennium_modem_model::take_ncc_rx(std::uint8_t &out)
{
	if (!m_dcd || m_ncc_rx.empty())
		return false;
	out = m_ncc_rx.front();
	m_ncc_rx.erase(m_ncc_rx.begin());
	return true;
}
