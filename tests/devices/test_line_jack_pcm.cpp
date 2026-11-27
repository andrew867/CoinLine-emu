// SPDX-License-Identifier: GPL-2.0-or-later
#include "millennium_line.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
using sock_type = SOCKET;
using addr_length = int;
constexpr sock_type invalid = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
using sock_type = int;
using addr_length = socklen_t;
constexpr sock_type invalid = -1;
#endif

namespace {
struct sock {
	sock_type value = invalid;
	void close()
	{
		if (value == invalid) return;
#if defined(_WIN32)
		::closesocket(value);
#else
		::close(value);
#endif
		value = invalid;
	}
	~sock() { close(); }
};

int fail(char const *message) { std::cerr << message << '\n'; return 1; }

sockaddr_in address(unsigned port)
{
	sockaddr_in value{};
	value.sin_family = AF_INET;
	value.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	value.sin_port = htons(static_cast<unsigned short>(port));
	return value;
}

bool readable(sock const &s)
{
	fd_set r;
	FD_ZERO(&r); FD_SET(s.value, &r);
	timeval tv{}; tv.tv_usec = 10000;
	return ::select(static_cast<int>(s.value) + 1, &r, nullptr, nullptr, &tv) > 0;
}

bool send_all(sock const &s, std::string const &data)
{
	for (std::size_t off = 0; off < data.size();) {
		int const n = ::send(s.value, data.data() + off, static_cast<int>(data.size() - off), 0);
		if (n <= 0) return false;
		off += static_cast<unsigned>(n);
	}
	return true;
}

std::string pcm_line(char prefix, std::int16_t value)
{
	constexpr char hex[] = "0123456789abcdef";
	std::string line(1, prefix);
	auto const v = static_cast<std::uint16_t>(value);
	for (unsigned i = 0; i < 160; ++i) {
		line += hex[(v >> 4) & 15U]; line += hex[v & 15U];
		line += hex[(v >> 12) & 15U]; line += hex[(v >> 8) & 15U];
	}
	return line + '\n';
}

void poll(millennium_line_jack &jack)
{
	for (int i = 0; i < 20; ++i) {
		jack.poll();
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}
}

int main()
{
#if defined(_WIN32)
	WSADATA data{};
	if (::WSAStartup(MAKEWORD(2, 2), &data) != 0) return fail("Winsock init failed");
#endif
	sock reservation;
	reservation.value = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	auto addr = address(0);
	addr_length size = sizeof(addr);
	if (reservation.value == invalid || ::bind(reservation.value, reinterpret_cast<sockaddr *>(&addr), size) != 0 ||
		::getsockname(reservation.value, reinterpret_cast<sockaddr *>(&addr), &size) != 0)
		return fail("cannot reserve loopback port");
	std::string const port = std::to_string(ntohs(addr.sin_port));
	reservation.close();
#if defined(_WIN32)
	_putenv_s("COINLINE_LINE_PORT", port.c_str());
#else
	setenv("COINLINE_LINE_PORT", port.c_str(), 1);
#endif
	millennium_line_jack jack;
	jack.open_from_env();
	if (!jack.enabled()) return fail("jack listen failed");
	jack.send_line("H1\n"); // no peer; the next attach reports current state instead
	sock peer;
	peer.value = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (peer.value == invalid || ::connect(peer.value, reinterpret_cast<sockaddr *>(&addr), size) != 0)
		return fail("jack connect failed");
	poll(jack);
	if (!jack.take_connected() || !jack.line_connected() || readable(peer))
		return fail("jack attach replayed stale controls or failed line presence");

	std::int16_t sample = 0;
	if (!send_all(peer, "B1\n" + pcm_line('W', 0x1234))) return fail("first PCM send failed");
	poll(jack);
	if (jack.take_incoming_pcm(sample)) return fail("real-time PCM started before two packets");
	if (!send_all(peer, pcm_line('W', -1234))) return fail("second PCM send failed");
	poll(jack);
	for (int i = 0; i < 320; ++i)
		if (!jack.take_incoming_pcm(sample) || sample != (i < 160 ? 0x1234 : -1234))
			return fail("PCM sequence changed");
	if (jack.take_incoming_pcm(sample)) return fail("unexpected input after two packets");

	for (int i = 0; i < 160; ++i) jack.push_outgoing_pcm(0x2468);
	jack.poll();
	jack.send_line("H1\n");
	std::string const expected = pcm_line('M', 0x2468) + "H1\n";
	std::string received;
	for (int i = 0; i < 20 && received.size() < expected.size(); ++i) {
		jack.poll();
		if (readable(peer)) {
			char buf[2048];
			int const n = ::recv(peer.value, buf, sizeof(buf), 0);
			if (n <= 0) return fail("unexpected peer EOF");
			received.append(buf, static_cast<std::size_t>(n));
		}
	}
	if (received != expected) return fail("PCM/control line ordering changed");
	if (!send_all(peer, "R1\nV1\nT1\n")) return fail("line controls send failed");
	poll(jack);
	if (!jack.ring_present() || jack.reversal() != 2 || !jack.co_dial_tone())
		return fail("line controls not applied");
	linger abortive{1, 0};
	if (::setsockopt(peer.value, SOL_SOCKET, SO_LINGER, reinterpret_cast<char const *>(&abortive), sizeof(abortive)) != 0)
		return fail("reset setup failed");
	peer.close();
	poll(jack);
	if (jack.line_connected() || jack.ring_present() || jack.reversal() || jack.co_dial_tone())
		return fail("hard peer loss did not clear line controls");
	jack.close();
	std::cout << "line jack PCM priming, order, stale-control and hard-disconnect checks passed\n";
	return 0;
}
