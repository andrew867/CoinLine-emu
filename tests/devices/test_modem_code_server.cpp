// SPDX-License-Identifier: GPL-2.0-or-later
#include "millennium_modem_model.h"

#include <array>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
using socket_type = SOCKET;
using address_length = int;
constexpr socket_type invalid_socket = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_type = int;
using address_length = socklen_t;
constexpr socket_type invalid_socket = -1;
#endif

namespace {

struct socket_handle {
	socket_type value = invalid_socket;
	~socket_handle()
	{
		if (value == invalid_socket)
			return;
#if defined(_WIN32)
		closesocket(value);
#else
		close(value);
#endif
	}
};

bool ready(socket_type socket, long milliseconds = 1000)
{
	fd_set reads;
	FD_ZERO(&reads);
	FD_SET(socket, &reads);
	timeval timeout{};
	timeout.tv_sec = milliseconds / 1000;
	timeout.tv_usec = (milliseconds % 1000) * 1000;
	return select(static_cast<int>(socket) + 1, &reads, nullptr, nullptr, &timeout) > 0;
}

unsigned listen_local(socket_handle &listener)
{
	listener.value = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listener.value == invalid_socket)
		return 0;
	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	address_length size = sizeof(addr);
	if (bind(listener.value, reinterpret_cast<sockaddr *>(&addr), size) != 0
		|| listen(listener.value, 1) != 0
		|| getsockname(listener.value, reinterpret_cast<sockaddr *>(&addr), &size) != 0)
		return 0;
	return ntohs(addr.sin_port);
}

void set_config(char const *name, std::string const &value)
{
#if defined(_WIN32)
	_putenv_s(name, value.c_str());
#else
	setenv(name, value.c_str(), 1);
#endif
}

int fail(char const *reason)
{
	std::cerr << reason << '\n';
	return 1;
}

void dial(millennium_modem_model &modem, char const *digits)
{
	modem.note_modem_relay(true, 0.0);
	double now = 0.0;
	for (char const *digit = digits; *digit; ++digit) {
		modem.note_outpulse_digit(*digit, now);
		now += 0.1;
	}
	modem.poll_ncc_link(3.0);
}

} // namespace

int main()
{
#if defined(_WIN32)
	WSADATA w{};
	if (WSAStartup(MAKEWORD(2, 2), &w) != 0)
		return fail("Cannot initialize sockets");
#endif
	socket_handle ncc_listener, code_listener;
	unsigned ncc_port = listen_local(ncc_listener);
	unsigned code_port = listen_local(code_listener);
	if (!ncc_port || !code_port)
		return fail("Cannot start local listeners");
	set_config("COINLINE_NCC_PORT", std::to_string(ncc_port));
	set_config("COINLINE_CODE_SERVER_NUMBER", "2130706434");
	set_config("COINLINE_CODE_SERVER_PORT", std::to_string(code_port));
	set_config("COINLINE_NCC_LAB_HELLO", "1");

	millennium_modem_model modem;
	modem.reset();
	dial(modem, "2130706435");
	if (modem.ncc_link_up() || ready(ncc_listener.value, 20) || ready(code_listener.value, 20))
		return fail("Unconfigured number opened a data call");
	modem.note_modem_relay(false, 3.1);

	dial(modem, "2130706434");
	if (!modem.ncc_link_up() || modem.dcd() || !ready(code_listener.value) || ready(ncc_listener.value, 20))
		return fail("Code-server digits did not select the separate local port");
	socket_handle code_call{accept(code_listener.value, nullptr, nullptr)};
	if (code_call.value == invalid_socket)
		return fail("Cannot accept code-server call");
	modem.poll_ncc_link(3.5);
	if (ready(code_call.value, 20))
		return fail("NCC fixture bytes leaked into the code-server call");
	modem.poll_ncc_link(8.1);
	if (!modem.dcd())
		return fail("Code-server call did not provide carrier");

	std::array<char, 4> const bytes{0x01, 0x00, char(0x80), char(0xff)};
	if (send(code_call.value, bytes.data(), static_cast<int>(bytes.size()), 0) != int(bytes.size()))
		return fail("Could not send test data to the modem");
	for (char expected : bytes) {
		std::uint8_t received = 0;
		bool obtained = false;
		for (int attempt = 0; attempt < 100 && !obtained; ++attempt) {
			modem.poll_ncc_link(8.2);
			obtained = modem.take_ncc_rx(received);
			if (!obtained)
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		if (!obtained || received != static_cast<std::uint8_t>(expected))
			return fail("Code-server receive stream was altered");
		modem.note_tx(received);
	}
	modem.poll_ncc_link(8.3);
	std::array<char, 4> returned{};
	for (std::size_t offset = 0; offset < returned.size();) {
		if (!ready(code_call.value))
			return fail("No code-server transmit data");
		int const count = recv(code_call.value, returned.data() + offset,
			static_cast<int>(returned.size() - offset), 0);
		if (count <= 0)
			return fail("Code-server transmit stream ended early");
		offset += count;
	}
	if (returned != bytes)
		return fail("Code-server transmit stream was altered");
	modem.note_modem_relay(false, 9.0);
	if (modem.ncc_link_up() || modem.dcd() || !ready(code_call.value)
		|| recv(code_call.value, returned.data(), 1, 0) != 0)
		return fail("Releasing the relay did not end the code-server call");

	set_config("COINLINE_NCC_LAB_HELLO", "0");
	dial(modem, "2130706433");
	if (!modem.ncc_link_up() || !ready(ncc_listener.value) || ready(code_listener.value, 20))
		return fail("Normal NCC dial no longer selects the normal port");
	socket_handle ncc_call{accept(ncc_listener.value, nullptr, nullptr)};
	if (ncc_call.value == invalid_socket)
		return fail("Cannot accept ordinary NCC call");
	modem.note_modem_relay(false, 4.0);
	return 0;
}
