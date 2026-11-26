// SPDX-License-Identifier: GPL-2.0-or-later
#include "millennium_modem_model.h"

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
	void close_socket()
	{
		if (value == invalid_socket)
			return;
#if defined(_WIN32)
		::closesocket(value);
#else
		::close(value);
#endif
		value = invalid_socket;
	}
	~socket_handle() { close_socket(); }
};

int fail(char const *why)
{
	std::cerr << why << '\n';
	return 1;
}

unsigned bind_local(socket_handle &server)
{
	server.value = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (server.value == invalid_socket)
		return 0;
	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	address_length size = sizeof(addr);
	if (::bind(server.value, reinterpret_cast<sockaddr *>(&addr), size) != 0
		|| ::getsockname(server.value, reinterpret_cast<sockaddr *>(&addr), &size) != 0)
		return 0;
	return ntohs(addr.sin_port);
}

void configure(char const *name, std::string const &value)
{
#if defined(_WIN32)
	_putenv_s(name, value.c_str());
#else
	setenv(name, value.c_str(), 1);
#endif
}

void dial(millennium_modem_model &modem)
{
	modem.reset();
	modem.set_line_present(true);
	modem.note_modem_relay(true, 0.0);
	double now = 0.0;
	for (char const digit : std::string("2130706433")) {
		modem.note_outpulse_digit(digit, now);
		now += 0.1;
	}
	modem.poll_ncc_link(3.0);
}

bool accept_local(socket_handle const &server, socket_handle &peer)
{
	fd_set reads;
	FD_ZERO(&reads);
	FD_SET(server.value, &reads);
	timeval tv{};
	tv.tv_sec = 1;
	if (::select(static_cast<int>(server.value) + 1, &reads, nullptr, nullptr, &tv) <= 0)
		return false;
	peer.value = ::accept(server.value, nullptr, nullptr);
	return peer.value != invalid_socket;
}

bool readable(socket_handle const &peer)
{
	fd_set reads;
	FD_ZERO(&reads);
	FD_SET(peer.value, &reads);
	timeval tv{};
	tv.tv_usec = 20000;
	return ::select(static_cast<int>(peer.value) + 1, &reads, nullptr, nullptr, &tv) > 0;
}

bool wait_for_loss(millennium_modem_model &modem)
{
	for (unsigned i = 0; i < 100; ++i) {
		modem.poll_ncc_link(9.1 + 0.001 * i);
		if (!modem.ncc_link_up())
			return !modem.dcd() && modem.state() == millennium_modem_state::carrier_lost;
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	return false;
}

} // namespace

int main()
{
#if defined(_WIN32)
	WSADATA data{};
	if (::WSAStartup(MAKEWORD(2, 2), &data) != 0)
		return fail("Winsock setup failed");
#endif
	// The removed bring-up switch must no longer inject bytes into a native
	// terminal session, even when inherited from an old lab shell.
	configure("COINLINE_NCC_LAB_HELLO", "1");
	socket_handle server;
	unsigned const port = bind_local(server);
	if (!port)
		return fail("loopback bind failed");
	configure("COINLINE_NCC_PORT", std::to_string(port));

	// The bound socket is not listening: a pending or refused connect must
	// not advertise an established NCC transport or carrier.
	millennium_modem_model modem;
	dial(modem);
	if (modem.ncc_link_up() || modem.dcd())
		return fail("refused connection counted as connected");
	if (::listen(server.value, 2) != 0)
		return fail("loopback listen failed");
	// The peer becomes available while the same dial and R1 seize remain
	// active. Refusal must not permanently suppress the retry timer.
	modem.poll_ncc_link(3.5);
	if (modem.ncc_link_up() || modem.dcd() || readable(server))
		return fail("refused connection retried before its deadline");
	modem.poll_ncc_link(4.01);
	{
		socket_handle peer;
		if (!modem.ncc_link_up() || modem.dcd() || !accept_local(server, peer))
			return fail("refused connection did not retry after its deadline");
		modem.poll_ncc_link(9.02);
		if (!modem.dcd())
			return fail("retried TCP connection did not establish normal carrier");
		peer.close_socket();
		if (!wait_for_loss(modem))
			return fail("retried connection did not report peer closure");
		modem.poll_ncc_link(15.0);
		if (modem.ncc_link_up() || modem.dcd() || readable(server))
			return fail("established call silently reconnected after peer closure");
		modem.note_modem_relay(false, 16.0);
	}

	for (bool reset_peer : {false, true}) {
		dial(modem);
		socket_handle peer;
		if (!modem.ncc_link_up() || modem.dcd() || !accept_local(server, peer))
			return fail("valid TCP handshake not established");
		// No bytes are pending. A nonblocking receive that would block must
		// leave the established transport and carrier intact.
		modem.poll_ncc_link(8.1);
		modem.poll_ncc_link(9.0);
		if (!modem.ncc_link_up() || !modem.dcd())
			return fail("idle nonblocking receive dropped carrier");
		if (readable(peer))
			return fail("unsolicited data on native NCC connection");
		modem.note_tx(0xa5);
		modem.poll_ncc_link(9.01);
		if (!readable(peer))
			return fail("guest TX was not forwarded");
		char received = 0;
		if (::recv(peer.value, &received, 1, 0) != 1 || static_cast<std::uint8_t>(received) != 0xa5)
			return fail("guest TX changed on the wire");
		char const reply = 0x5a;
		if (::send(peer.value, &reply, 1, 0) != 1)
			return fail("loopback reply failed");
		std::uint8_t guest_rx = 0;
		bool got_reply = false;
		for (unsigned i = 0; i < 50 && !got_reply; ++i) {
			modem.poll_ncc_link(9.02 + 0.001 * i);
			got_reply = modem.take_ncc_rx(guest_rx);
			if (!got_reply)
				std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}
		if (!got_reply || guest_rx != 0x5a)
			return fail("host RX changed or not delivered");

		if (reset_peer) {
			// A local abortive close produces a TCP reset, not orderly EOF.
			linger abortive{1, 0};
			if (::setsockopt(peer.value, SOL_SOCKET, SO_LINGER,
				reinterpret_cast<char const *>(&abortive), sizeof(abortive)) != 0)
				return fail("could not configure loopback reset");
		} else {
#if defined(_WIN32)
			if (::shutdown(peer.value, SD_SEND) != 0)
#else
			if (::shutdown(peer.value, SHUT_WR) != 0)
#endif
				return fail("orderly close failed");
		}
		peer.close_socket();
		if (!wait_for_loss(modem))
			return fail(reset_peer ? "hard receive error did not drop carrier" : "EOF did not drop carrier");
		modem.note_modem_relay(false, 10.0);
	}
	// Releasing the physical relay must cancel a pending refused-connect
	// retry, even if the previously unavailable peer comes up afterward.
	socket_handle canceled_server;
	unsigned const canceled_port = bind_local(canceled_server);
	if (!canceled_port)
		return fail("canceled retry loopback bind failed");
	configure("COINLINE_NCC_PORT", std::to_string(canceled_port));
	dial(modem);
	if (modem.ncc_link_up() || modem.dcd())
		return fail("second refused connection counted as connected");
	modem.note_modem_relay(false, 3.5);
	if (::listen(canceled_server.value, 2) != 0)
		return fail("canceled retry loopback listen failed");
	modem.poll_ncc_link(5.0);
	if (modem.ncc_link_up() || modem.dcd() || readable(canceled_server))
		return fail("relay release did not cancel pending connect retry");
	std::cout << "NCC TCP refused connect, timed retry, no silent redial, would-block, EOF and reset passed\n";
	return 0;
}
