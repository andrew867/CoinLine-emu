// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_hostbridge_tcp.h"

#include <chrono>
#include <cstring>
#include <mutex>
#include <thread>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {

std::once_flag g_net_once;

void net_init_once()
{
#if defined(_WIN32)
	WSADATA w{};
	(void)::WSAStartup(MAKEWORD(2, 2), &w);
#else
#endif
}

#if defined(_WIN32)
SOCKET as_sock(std::intptr_t v)
{
	return static_cast<SOCKET>(static_cast<std::uintptr_t>(v));
}

std::intptr_t from_sock(SOCKET s)
{
	return static_cast<std::intptr_t>(static_cast<std::uintptr_t>(s));
}

bool sock_invalid(std::intptr_t v)
{
	return as_sock(v) == INVALID_SOCKET;
}
#else
int as_sock(std::intptr_t v)
{
	return int(v);
}

std::intptr_t from_sock(int s)
{
	return std::intptr_t(s);
}

bool sock_invalid(std::intptr_t v)
{
	return v < 0;
}
#endif

void set_recv_timeout_ms(std::intptr_t sock, int ms)
{
#if defined(_WIN32)
	SOCKET s = as_sock(sock);
	DWORD tv = DWORD(ms);
	(void)::setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char const *>(&tv), sizeof(tv));
#else
	struct timeval tv {};
	tv.tv_sec = ms / 1000;
	tv.tv_usec = (ms % 1000) * 1000;
	int fd = as_sock(sock);
	(void)::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
}

} // namespace

millennium_hostbridge_tcp::millennium_hostbridge_tcp()
{
	std::call_once(g_net_once, net_init_once);
}

millennium_hostbridge_tcp::~millennium_hostbridge_tcp()
{
	close_socket();
#if defined(_WIN32)
	if (!sock_invalid(m_listen))
		::closesocket(as_sock(m_listen));
#else
	if (!sock_invalid(m_listen))
		::close(as_sock(m_listen));
#endif
	m_listen = -1;
}

bool millennium_hostbridge_tcp::ensure_started()
{
	std::call_once(g_net_once, net_init_once);
	return true;
}

bool millennium_hostbridge_tcp::listen_ipv4_local(std::uint16_t port_wish, std::uint16_t *bound_port_out)
{
	ensure_started();
	close_socket();
#if defined(_WIN32)
	if (!sock_invalid(m_listen))
		::closesocket(as_sock(m_listen));
	m_listen = -1;
	SOCKET ls = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (ls == INVALID_SOCKET)
		return false;
	BOOL yes = 1;
	::setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char const *>(&yes), sizeof(yes));
	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = htons(port_wish);
	if (::bind(ls, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
		::closesocket(ls);
		return false;
	}
	if (::listen(ls, 1) != 0) {
		::closesocket(ls);
		return false;
	}
	m_listen = from_sock(ls);
	sockaddr_in out{};
	int len = sizeof(out);
	if (::getsockname(ls, reinterpret_cast<sockaddr *>(&out), &len) == 0 && bound_port_out)
		*bound_port_out = ntohs(out.sin_port);
#else
	if (!sock_invalid(m_listen))
		::close(as_sock(m_listen));
	m_listen = -1;
	int ls = ::socket(AF_INET, SOCK_STREAM, 0);
	if (ls < 0)
		return false;
	int yes = 1;
	::setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = htons(port_wish);
	if (::bind(ls, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
		::close(ls);
		return false;
	}
	if (::listen(ls, 1) != 0) {
		::close(ls);
		return false;
	}
	m_listen = from_sock(ls);
	sockaddr_in out{};
	socklen_t len = sizeof(out);
	if (::getsockname(ls, reinterpret_cast<sockaddr *>(&out), &len) == 0 && bound_port_out)
		*bound_port_out = ntohs(out.sin_port);
#endif
	return true;
}

bool millennium_hostbridge_tcp::accept_blocking(int timeout_ms)
{
#if defined(_WIN32)
	if (sock_invalid(m_listen))
		return false;
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(as_sock(m_listen), &fds);
	timeval tv{};
	tv.tv_sec = timeout_ms / 1000;
	tv.tv_usec = (timeout_ms % 1000) * 1000;
	int r = ::select(0, &fds, nullptr, nullptr, &tv);
	if (r <= 0)
		return false;
	SOCKET c = ::accept(as_sock(m_listen), nullptr, nullptr);
	if (c == INVALID_SOCKET)
		return false;
	m_sock = from_sock(c);
	return true;
#else
	if (sock_invalid(m_listen))
		return false;
	struct pollfd pfd {};
	pfd.fd = as_sock(m_listen);
	pfd.events = POLLIN;
	if (::poll(&pfd, 1, timeout_ms) <= 0)
		return false;
	int c = ::accept(as_sock(m_listen), nullptr, nullptr);
	if (c < 0)
		return false;
	m_sock = from_sock(c);
	return true;
#endif
}

bool millennium_hostbridge_tcp::connect_ipv4(std::string const &host, std::uint16_t port)
{
	ensure_started();
	close_socket();
#if defined(_WIN32)
	SOCKET s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s == INVALID_SOCKET)
		return false;
	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
		::closesocket(s);
		return false;
	}
	if (::connect(s, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
		::closesocket(s);
		return false;
	}
	m_sock = from_sock(s);
	return true;
#else
	int s = ::socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0)
		return false;
	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
		::close(s);
		return false;
	}
	if (::connect(s, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
		::close(s);
		return false;
	}
	m_sock = from_sock(s);
	return true;
#endif
}

void millennium_hostbridge_tcp::close_socket()
{
#if defined(_WIN32)
	if (!sock_invalid(m_sock))
		::closesocket(as_sock(m_sock));
#else
	if (!sock_invalid(m_sock))
		::close(as_sock(m_sock));
#endif
	m_sock = -1;
}

bool millennium_hostbridge_tcp::send_bytes(std::vector<std::uint8_t> const &data)
{
#if defined(_WIN32)
	if (sock_invalid(m_sock))
		return false;
	std::size_t off = 0;
	while (off < data.size()) {
		int r = ::send(as_sock(m_sock), reinterpret_cast<char const *>(data.data() + off),
			int(data.size() - off), 0);
		if (r <= 0)
			return false;
		off += std::size_t(r);
	}
	return true;
#else
	if (sock_invalid(m_sock))
		return false;
	std::size_t off = 0;
	while (off < data.size()) {
		ssize_t r = ::send(as_sock(m_sock), data.data() + off, data.size() - off, 0);
		if (r <= 0)
			return false;
		off += std::size_t(r);
	}
	return true;
#endif
}

bool millennium_hostbridge_tcp::recv_append_available(std::vector<std::uint8_t> &buffer)
{
	std::uint8_t chunk[4096];
#if defined(_WIN32)
	if (sock_invalid(m_sock))
		return false;
	set_recv_timeout_ms(m_sock, 50);
	int r = ::recv(as_sock(m_sock), reinterpret_cast<char *>(chunk), sizeof(chunk), 0);
	if (r > 0)
		buffer.insert(buffer.end(), chunk, chunk + r);
	return r > 0;
#else
	if (sock_invalid(m_sock))
		return false;
	set_recv_timeout_ms(m_sock, 50);
	ssize_t r = ::recv(as_sock(m_sock), chunk, sizeof(chunk), 0);
	if (r > 0)
		buffer.insert(buffer.end(), chunk, chunk + r);
	return r > 0;
#endif
}

std::vector<std::uint8_t> millennium_hostbridge_tcp::frame_json(std::string const &json_utf8)
{
	std::vector<std::uint8_t> out;
	std::uint32_t len = static_cast<std::uint32_t>(json_utf8.size());
	out.push_back(static_cast<std::uint8_t>((len >> 24) & 0xff));
	out.push_back(static_cast<std::uint8_t>((len >> 16) & 0xff));
	out.push_back(static_cast<std::uint8_t>((len >> 8) & 0xff));
	out.push_back(static_cast<std::uint8_t>(len & 0xff));
	out.insert(out.end(), json_utf8.begin(), json_utf8.end());
	return out;
}

bool millennium_hostbridge_tcp::parse_length_prefix(std::vector<std::uint8_t> const &buf,
	std::size_t &consumed_bytes, std::string &json_out)
{
	consumed_bytes = 0;
	json_out.clear();
	if (buf.size() < 4)
		return false;
	std::uint32_t len = (std::uint32_t(buf[0]) << 24) | (std::uint32_t(buf[1]) << 16)
		| (std::uint32_t(buf[2]) << 8) | std::uint32_t(buf[3]);
	if (len > 1024 * 1024)
		return false;
	if (buf.size() < 4 + len)
		return false;
	json_out.assign(reinterpret_cast<char const *>(buf.data() + 4), len);
	consumed_bytes = 4 + std::size_t(len);
	return true;
}

bool millennium_hostbridge_tcp::send_sideband_json(std::string const &json_utf8)
{
	m_negotiate_sent = true;
	auto framed = frame_json(json_utf8);
	return send_bytes(framed);
}

bool millennium_hostbridge_tcp::try_recv_sideband_json(std::string &json_out, int timeout_ms)
{
	json_out.clear();
	std::vector<std::uint8_t> acc;
#if defined(_WIN32)
	if (sock_invalid(m_sock))
		return false;
#else
	if (sock_invalid(m_sock))
		return false;
#endif
	using clock = std::chrono::steady_clock;
	auto const deadline = clock::now() + std::chrono::milliseconds(timeout_ms);
	while (clock::now() < deadline) {
		std::vector<std::uint8_t> chunk;
		(void)recv_append_available(chunk);
		acc.insert(acc.end(), chunk.begin(), chunk.end());
		std::size_t consumed = 0;
		if (parse_length_prefix(acc, consumed, json_out) && consumed <= acc.size())
			return true;
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	return false;
}
