// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_hostbridge_tcp.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

int main()
{
	std::atomic<std::uint16_t> port_out{0};
	std::atomic<bool> srv_ok{true};
	std::thread server([&]() {
		millennium_hostbridge_tcp s;
		std::uint16_t bound = 0;
		if (!s.listen_ipv4_local(0, &bound)) {
			srv_ok = false;
			return;
		}
		port_out.store(bound, std::memory_order_relaxed);
		if (!s.accept_blocking(15000)) {
			srv_ok = false;
			return;
		}
		std::vector<std::uint8_t> acc;
		auto const start = std::chrono::steady_clock::now();
		while (acc.size() < 4
			&& std::chrono::steady_clock::now() - start < std::chrono::seconds(8)) {
			std::vector<std::uint8_t> chunk;
			if (s.recv_append_available(chunk))
				acc.insert(acc.end(), chunk.begin(), chunk.end());
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}
		if (acc.size() != 4) {
			srv_ok = false;
			return;
		}
		if (!s.send_bytes(acc))
			srv_ok = false;
		s.close_socket();
	});

	while (port_out.load(std::memory_order_relaxed) == 0)
		std::this_thread::sleep_for(std::chrono::milliseconds(5));

	millennium_hostbridge_tcp client;
	if (!client.connect_ipv4("127.0.0.1", port_out.load(std::memory_order_relaxed))) {
		std::cerr << "connect failed\n";
		srv_ok = false;
	}
	std::vector<std::uint8_t> const out = {0x01, 0x02, 0x03, 0x04};
	if (!client.send_bytes(out)) {
		std::cerr << "client send failed\n";
		srv_ok = false;
	}
	std::vector<std::uint8_t> in;
	auto const t0 = std::chrono::steady_clock::now();
	while (in.size() < 4 && std::chrono::steady_clock::now() - t0 < std::chrono::seconds(8)) {
		std::vector<std::uint8_t> chunk;
		if (client.recv_append_available(chunk))
			in.insert(in.end(), chunk.begin(), chunk.end());
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	client.close_socket();
	server.join();

	if (!srv_ok.load()) {
		std::cerr << "server path failed\n";
		return 1;
	}
	if (in != out) {
		std::cerr << "round-trip mismatch\n";
		return 1;
	}
	return 0;
}
