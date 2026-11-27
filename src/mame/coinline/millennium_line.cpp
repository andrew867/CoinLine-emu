// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_line.h"

#include <cstdio>
#include <cerrno>
#include <cstdlib>
#include <cstring>

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
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {
#if defined(MSG_NOSIGNAL)
constexpr int bridge_send_flags = MSG_NOSIGNAL;
#else
constexpr int bridge_send_flags = 0;
#endif

void suppress_socket_sigpipe(int sock)
{
#if defined(SO_NOSIGPIPE)
	int enabled = 1;
	(void)::setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled));
#else
	(void)sock;
#endif
}
} // namespace
#endif

namespace {

#if defined(_WIN32)
SOCKET line_sock(std::intptr_t v)
{
	return static_cast<SOCKET>(static_cast<std::uintptr_t>(v));
}
bool line_bad(std::intptr_t v)
{
	return v < 0 || line_sock(v) == INVALID_SOCKET;
}
bool line_retryable_error()
{
	int const error = ::WSAGetLastError();
	return error == WSAEWOULDBLOCK || error == WSAEINTR;
}
#else
bool line_bad(std::intptr_t v)
{
	return v < 0;
}
bool line_retryable_error()
{
	return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR;
}
#endif

} // namespace

void millennium_line_jack::open_from_env()
{
	if (!line_bad(m_listen))
		return;
	char const *env = std::getenv("COINLINE_LINE_PORT");
	if (!env || !*env)
		return;
	int const port = std::atoi(env);
	if (port <= 0 || port > 65535)
		return;
#if defined(_WIN32)
	WSADATA w{};
	(void)::WSAStartup(MAKEWORD(2, 2), &w);
	SOCKET s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s == INVALID_SOCKET)
		return;
	u_long nb = 1;
	if (::ioctlsocket(s, FIONBIO, &nb) != 0) {
		::closesocket(s);
		return;
	}
	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(static_cast<u_short>(port));
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (::bind(s, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0 || ::listen(s, 1) != 0) {
		::closesocket(s);
		return;
	}
	m_listen = static_cast<std::intptr_t>(s);
#else
	int s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s < 0)
		return;
	int const flags = ::fcntl(s, F_GETFL, 0);
	if (flags < 0 || ::fcntl(s, F_SETFL, flags | O_NONBLOCK) != 0) {
		::close(s);
		return;
	}
	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(static_cast<uint16_t>(port));
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (::bind(s, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0 || ::listen(s, 1) != 0) {
		::close(s);
		return;
	}
	m_listen = s;
#endif
	std::fprintf(stderr, "line jack %d\n", port);
}

void millennium_line_jack::disconnect_client() noexcept
{
#if defined(_WIN32)
	if (!line_bad(m_client))
		::closesocket(line_sock(m_client));
#else
	if (!line_bad(m_client))
		::close(static_cast<int>(m_client));
#endif
	m_client = -1;
	m_rx.clear();
	m_tx_pending.clear();
	m_ring = false;
	m_connected = false;
	m_co_dial_tone = false;
	m_reversal = 0;
	m_connected_edge = false;
	std::lock_guard<std::mutex> lock(m_pcm_mu);
	m_pcm_to_co.clear();
	m_pcm_from_co.clear();
	m_pcm_realtime = m_pcm_primed = false;
}

void millennium_line_jack::close() noexcept
{
	disconnect_client();
#if defined(_WIN32)
	if (!line_bad(m_listen))
		::closesocket(line_sock(m_listen));
#else
	if (!line_bad(m_listen))
		::close(static_cast<int>(m_listen));
#endif
	m_listen = -1;
}

void millennium_line_jack::drain_tx()
{
	if (line_bad(m_client) || m_tx_pending.empty())
		return;
#if defined(_WIN32)
	int const n = ::send(line_sock(m_client), m_tx_pending.data(), static_cast<int>(m_tx_pending.size()), 0);
#else
	int const n = ::send(static_cast<int>(m_client), m_tx_pending.data(), m_tx_pending.size(), bridge_send_flags);
#endif
	if (n > 0)
		m_tx_pending.erase(0, static_cast<std::size_t>(n));
	else if (n < 0 && !line_retryable_error())
		disconnect_client();
}

void millennium_line_jack::send_line(char const *text)
{
	if (!text || !*text || line_bad(m_client))
		return;
	m_tx_pending.append(text);
	// Never discard a partial control or media line from a live stream.
	// A peer stalled this far has lost the real-time line; reconnect cleanly.
	if (m_tx_pending.size() > 65536U) {
		disconnect_client();
		return;
	}
	drain_tx();
}

bool millennium_line_jack::take_connected() noexcept
{
	bool const v = m_connected_edge;
	m_connected_edge = false;
	return v;
}

void millennium_line_jack::push_outgoing_pcm(std::int16_t sample)
{
	std::lock_guard<std::mutex> lock(m_pcm_mu);
	if (m_pcm_to_co.size() >= 8000U)
		m_pcm_to_co.pop_front();
	m_pcm_to_co.push_back(sample);
}

bool millennium_line_jack::take_incoming_pcm(std::int16_t &sample)
{
	std::lock_guard<std::mutex> lock(m_pcm_mu);
	if (m_pcm_realtime && !m_pcm_primed) {
		// Two 20 ms frames keep packet-arrival phase separate from the
		// analogue sample clock without a large call-start delay.
		if (m_pcm_from_co.size() < 320U) return false;
		m_pcm_primed = true;
	}
	if (m_pcm_from_co.empty()) {
		m_pcm_primed = false;
		return false;
	}
	sample = m_pcm_from_co.front();
	m_pcm_from_co.pop_front();
	return true;
}

void millennium_line_jack::ingest_pcm_line(std::string const &hex)
{
	std::lock_guard<std::mutex> lock(m_pcm_mu);
	for (std::size_t i = 0; i + 3 < hex.size() && m_pcm_from_co.size() < 8000U; i += 4) {
		auto nyb = [](char c) -> int {
			if (c >= '0' && c <= '9')
				return c - '0';
			if (c >= 'a' && c <= 'f')
				return c - 'a' + 10;
			if (c >= 'A' && c <= 'F')
				return c - 'A' + 10;
			return -1;
		};
		int const a = nyb(hex[i]);
		int const b = nyb(hex[i + 1]);
		int const c = nyb(hex[i + 2]);
		int const d = nyb(hex[i + 3]);
		if (a < 0 || b < 0 || c < 0 || d < 0)
			return;
		unsigned const lo = unsigned((a << 4) | b);
		unsigned const hi = unsigned((c << 4) | d);
		m_pcm_from_co.push_back(static_cast<std::int16_t>(lo | (hi << 8)));
	}
}

void millennium_line_jack::flush_outgoing_pcm()
{
	// Do not accumulate encoded audio in the TCP queue during backpressure.
	// The bounded PCM queue retains samples until the previous line drains.
	if (!m_tx_pending.empty())
		return;
	std::string hex;
	{
		std::lock_guard<std::mutex> lock(m_pcm_mu);
		if (m_pcm_to_co.size() < 160U)
			return;
		hex.reserve(1 + 160U * 4 + 1);
		hex.push_back('M');
		char nyb[] = "0123456789abcdef";
		for (int n = 0; n < 160; ++n) {
			unsigned const s = static_cast<std::uint16_t>(m_pcm_to_co.front());
			m_pcm_to_co.pop_front();
			hex.push_back(nyb[(s >> 4) & 0xf]);
			hex.push_back(nyb[s & 0xf]);
			hex.push_back(nyb[(s >> 12) & 0xf]);
			hex.push_back(nyb[(s >> 8) & 0xf]);
		}
	}
	hex.push_back('\n');
	send_line(hex.c_str());
}

void millennium_line_jack::poll()
{
	if (line_bad(m_listen))
		return;
#if defined(_WIN32)
	if (line_bad(m_client)) {
		SOCKET c = ::accept(line_sock(m_listen), nullptr, nullptr);
		if (c != INVALID_SOCKET) {
			u_long nb = 1;
			if (::ioctlsocket(c, FIONBIO, &nb) != 0) {
				::closesocket(c);
				return;
			}
			int const no_delay = 1;
			(void)::setsockopt(c, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char const *>(&no_delay), sizeof(no_delay));
			m_client = static_cast<std::intptr_t>(c);
			m_connected_edge = true;
			// CO attached ⇒ tip/ring battery present for QUERY_CO_LINE_STATUS (craft Failed:41).
			m_connected = true;
			m_rx.clear();
			std::lock_guard<std::mutex> lock(m_pcm_mu);
			m_pcm_to_co.clear();
			m_pcm_from_co.clear();
		}
	}
	if (line_bad(m_client))
		return;
	drain_tx();
	if (line_bad(m_client))
		return;
	flush_outgoing_pcm();
	if (line_bad(m_client))
		return;
	// A 20 ms PCM packet is 642 text bytes. Read at least one complete
	// packet per poll rather than spreading it over three host wakeups.
	char buf[2048];
	int n = ::recv(line_sock(m_client), buf, sizeof(buf), 0);
	if (n == 0) {
		disconnect_client();
		return;
	}
	if (n < 0) {
		if (!line_retryable_error())
			disconnect_client();
		return;
	}
#else
	if (line_bad(m_client)) {
		int c = ::accept(static_cast<int>(m_listen), nullptr, nullptr);
		if (c >= 0) {
			suppress_socket_sigpipe(c);
			int const no_delay = 1;
			(void)::setsockopt(c, IPPROTO_TCP, TCP_NODELAY, &no_delay, sizeof(no_delay));
			int const flags = ::fcntl(c, F_GETFL, 0);
			if (flags < 0 || ::fcntl(c, F_SETFL, flags | O_NONBLOCK) != 0) {
				::close(c);
				return;
			}
			m_client = c;
			m_connected_edge = true;
			m_connected = true;
			m_rx.clear();
			std::lock_guard<std::mutex> lock(m_pcm_mu);
			m_pcm_to_co.clear();
			m_pcm_from_co.clear();
		}
	}
	if (line_bad(m_client))
		return;
	drain_tx();
	if (line_bad(m_client))
		return;
	flush_outgoing_pcm();
	if (line_bad(m_client))
		return;
	char buf[2048];
	int n = ::recv(static_cast<int>(m_client), buf, sizeof(buf), 0);
	if (n == 0) {
		disconnect_client();
		return;
	}
	if (n < 0) {
		if (!line_retryable_error())
			disconnect_client();
		return;
	}
#endif
	m_rx.append(buf, buf + n);
	if (m_rx.size() > 65536U) {
		disconnect_client();
		return;
	}
	for (;;) {
		std::size_t const nl = m_rx.find('\n');
		if (nl == std::string::npos)
			break;
		std::string line = m_rx.substr(0, nl);
		m_rx.erase(0, nl + 1);
		while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
			line.pop_back();
		if (line == "R1")
			m_ring = true;
		else if (line == "R0")
			m_ring = false;
		else if (line == "C1")
			m_connected = true;
		else if (line == "C0")
			m_connected = false;
		else if (line == "V0")
			m_reversal = 1;
		else if (line == "V1")
			m_reversal = 2;
		else if (line == "V")
			m_reversal = 0;
		else if (line == "T1" || line == "T0") {
			m_co_dial_tone = line[1] == '1';
			std::lock_guard<std::mutex> lock(m_pcm_mu);
			m_pcm_from_co.clear();
			m_pcm_primed = false;
		}
		else if (line == "B1" || line == "B0") {
			std::lock_guard<std::mutex> lock(m_pcm_mu);
			m_pcm_realtime = line[1] == '1';
			m_pcm_primed = false;
			m_pcm_from_co.clear();
		}
		else if (line.size() > 1 && line[0] == 'W')
			ingest_pcm_line(line.substr(1));
	}
}
