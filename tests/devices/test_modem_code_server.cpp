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

std::uint8_t read_detect_register(millennium_modem_model &modem)
{
	// Native SSI read address, active-low read strobe, LSB first.
	constexpr std::uint8_t active = 0x28U;
	constexpr std::uint8_t clock = 0x02U;
	modem.ssi_pio_a_write(active | 0x04U | clock);
	modem.ssi_pio_a_write(active | clock);
	std::uint8_t result = modem.ssi_mpd_level() ? 1U : 0U;
	for (unsigned bit = 1; bit < 8; ++bit) {
		modem.ssi_pio_a_write(active);
		modem.ssi_pio_a_write(active | clock);
		if (modem.ssi_mpd_level())
			result |= static_cast<std::uint8_t>(1U << bit);
	}
	modem.ssi_pio_a_write(active | 0x04U | clock);
	return result;
}

void write_register(millennium_modem_model &modem, std::uint8_t address, std::uint8_t value)
{
	constexpr std::uint8_t clock = 0x02U;
	std::uint8_t const base = (address << 4) | 0x04U;
	for (unsigned bit = 0; bit < 8; ++bit) {
		std::uint8_t const level = base | 0x08U | (((value >> bit) & 1U) ^ 1U);
		modem.ssi_pio_a_write(level | clock, true);
		modem.ssi_pio_a_write(level, true);
	}
	modem.ssi_pio_a_write(base | clock, true);
	modem.ssi_pio_a_write(base | 0x08U | clock, true);
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
	if (read_detect_register(modem) != 0x02U || modem.dcd())
		return fail("Code-server call did not expose ringback before answer");
	modem.poll_ncc_link(5.1);
	if (read_detect_register(modem) != 0x04U || modem.dcd())
		return fail("Code-server answer-tone phase was not visible on the SSI bus");
	modem.poll_ncc_link(8.34);
	if (read_detect_register(modem) != 0x00U || modem.dcd())
		return fail("Code-server answer turn-off gap must be observable");
	modem.poll_ncc_link(8.39);
	if (read_detect_register(modem) != 0x08U || modem.dcd())
		return fail("DPSK carrier acquisition is independent of mark qualification");
	modem.poll_ncc_link(8.41);
	if (read_detect_register(modem) != 0x38U || modem.dcd())
		return fail("Code-server unscrambled mark detector must be distinct from receive data");
	modem.poll_ncc_link(15.0);
	if (modem.dcd() || read_detect_register(modem) != 0x38U)
		return fail("A TCP connection and elapsed time alone cannot complete training");
	modem.note_tx(0x55U);
	if (ready(code_call.value, 20))
		return fail("Guest data escaped before modem training completed");
	// Guest starts in DPSK originate mode and transmits the actual S1
	// pattern through the modem control registers. A remote S1 response
	// cannot arrive until the guest's complete burst ends.
	write_register(modem, 0, 0x13);
	write_register(modem, 1, 0x40);
	write_register(modem, 4, 0x10);
	modem.poll_ncc_link(15.001);
	modem.poll_ncc_link(15.101);
	if (read_detect_register(modem) != 0x38U || modem.dcd())
		return fail("Remote S1 arrived before the originating S1 finished");
	write_register(modem, 1, 0x80);
	modem.poll_ncc_link(15.102);
	modem.poll_ncc_link(15.128);
	if (read_detect_register(modem) != 0x48U || modem.dcd())
		return fail("Remote S1 pattern was not visible on the SSI pins");
	modem.poll_ncc_link(15.23);
	if (read_detect_register(modem) != 0x28U || modem.dcd())
		return fail("Scrambled marks must follow S1 without premature data carrier");
	modem.poll_ncc_link(15.703);
	modem.poll_ncc_link(16.00);
	if (modem.dcd())
		return fail("Peer must wait for actual guest QAM configuration");
	write_register(modem, 0, 0x53);
	modem.poll_ncc_link(16.001);
	modem.poll_ncc_link(16.02);
	if (!modem.dcd() || read_detect_register(modem) != 0x08U)
		return fail("Code-server call did not provide carrier");

	std::array<char, 4> const bytes{0x01, 0x00, char(0x80), char(0xff)};
	if (send(code_call.value, bytes.data(), static_cast<int>(bytes.size()), 0) != int(bytes.size()))
		return fail("Could not send test data to the modem");
	for (char expected : bytes) {
		std::uint8_t received = 0;
		bool obtained = false;
		for (int attempt = 0; attempt < 100 && !obtained; ++attempt) {
			modem.poll_ncc_link(16.1);
			obtained = modem.take_ncc_rx(received);
			if (!obtained)
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		if (!obtained || received != static_cast<std::uint8_t>(expected))
			return fail("Code-server receive stream was altered");
		modem.note_tx(received);
	}
	modem.poll_ncc_link(16.2);
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
	modem.note_modem_relay(false, 17.0);
	if (modem.ncc_link_up() || modem.dcd() || !ready(code_call.value)
		|| recv(code_call.value, returned.data(), 1, 0) != 0)
		return fail("Releasing the relay did not end the code-server call");

	// V.22 fallback uses scrambled 1200 bit/s marks without S1 or QAM.
	dial(modem, "2130706434");
	if (!ready(code_listener.value)) return fail("Cannot start fallback data call");
	socket_handle fallback_call{accept(code_listener.value, nullptr, nullptr)};
	if (fallback_call.value == invalid_socket) return fail("Cannot accept fallback data call");
	write_register(modem, 0, 0x13);
	write_register(modem, 1, 0x80);
	write_register(modem, 4, 0x00);
	modem.poll_ncc_link(8.41);
	modem.poll_ncc_link(8.69);
	if (read_detect_register(modem) != 0x28U || modem.dcd())
		return fail("V.22 fallback did not qualify the guest's scrambled marks");
	modem.poll_ncc_link(9.4);
	if (modem.dcd()) return fail("V.22 fallback completed before its remote training interval");
	modem.poll_ncc_link(9.46);
	if (!modem.dcd() || read_detect_register(modem) != 0x08U)
		return fail("V.22 fallback did not complete");
	modem.note_modem_relay(false, 9.5);

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
