// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_hostbridge_tcp.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

int main()
{
	std::atomic<std::uint16_t> port_out{0};
	std::atomic<bool> ok{true};

	std::thread server([&]() {
		millennium_hostbridge_tcp s;
		std::uint16_t bound = 0;
		if (!s.listen_ipv4_local(0, &bound)) {
			ok = false;
			return;
		}
		port_out.store(bound, std::memory_order_relaxed);
		if (!s.accept_blocking(20000)) {
			ok = false;
			return;
		}
		std::string const neg = R"({"type":"negotiate","version":"1.0","ts":"2026-05-03T00:00:00Z"})";
		if (!s.send_sideband_json(neg)) {
			ok = false;
			return;
		}
		std::string peer;
		if (!s.try_recv_sideband_json(peer, 5000)) {
			ok = false;
			return;
		}
		if (peer.find("negotiate") == std::string::npos) {
			ok = false;
			return;
		}
		s.set_sideband_enabled(true);
		s.close_socket();
	});

	while (port_out.load(std::memory_order_relaxed) == 0)
		std::this_thread::sleep_for(std::chrono::milliseconds(5));

	millennium_hostbridge_tcp client;
	if (!client.connect_ipv4("127.0.0.1", port_out.load(std::memory_order_relaxed))) {
		ok = false;
		server.join();
		std::cerr << "client connect failed\n";
		return 1;
	}
	std::string inbound;
	if (!client.try_recv_sideband_json(inbound, 8000))
		ok = false;
	std::string const reply = R"({"type":"negotiate","version":"1.0","ts":"2026-05-03T00:00:01Z"})";
	if (!client.send_sideband_json(reply))
		ok = false;
	client.set_sideband_enabled(true);
	client.close_socket();
	server.join();

	if (!ok.load()) {
		std::cerr << "side-band negotiation failed\n";
		return 1;
	}
	return 0;
}
