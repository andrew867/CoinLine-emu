// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Plain TCP byte pipe + optional length-prefixed JSON side-band frames (host-bridge.spec.md).

class millennium_hostbridge_tcp {
public:
	millennium_hostbridge_tcp();
	~millennium_hostbridge_tcp();

	millennium_hostbridge_tcp(millennium_hostbridge_tcp const &) = delete;
	millennium_hostbridge_tcp &operator=(millennium_hostbridge_tcp const &) = delete;

	bool listen_ipv4_local(std::uint16_t port_wish, std::uint16_t *bound_port_out);
	bool accept_blocking(int timeout_ms);
	bool connect_ipv4(std::string const &host, std::uint16_t port);

	bool send_bytes(std::vector<std::uint8_t> const &data);
	bool recv_append_available(std::vector<std::uint8_t> &buffer);

	void close_socket();

	bool send_sideband_json(std::string const &json_utf8);
	bool try_recv_sideband_json(std::string &json_out, int timeout_ms);

	bool negotiate_completed() const noexcept { return m_negotiate_sent; }
	bool sideband_enabled() const noexcept { return m_sideband_enabled; }
	void set_sideband_enabled(bool v) noexcept { m_sideband_enabled = v; }

	static std::vector<std::uint8_t> frame_json(std::string const &json_utf8);
	static bool parse_length_prefix(std::vector<std::uint8_t> const &buf, std::size_t &consumed_bytes,
		std::string &json_out);

private:
	bool ensure_started();

	std::intptr_t m_sock = -1;
	std::intptr_t m_listen = -1;

	bool m_negotiate_sent = false;
	bool m_sideband_enabled = false;
};
