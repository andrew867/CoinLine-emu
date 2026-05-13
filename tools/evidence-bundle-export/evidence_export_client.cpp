// SPDX-License-Identifier: GPL-2.0-or-later

#include "evidence_export_client.h"

#include "../common/coinline_base64.hpp"
#include "../common/coinline_flat_json.hpp"

#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <string>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {

std::once_flag g_net_once;

void net_init_once()
{
	std::call_once(g_net_once, [] {
#if defined(_WIN32)
		WSADATA w{};
		(void)::WSAStartup(MAKEWORD(2, 2), &w);
#endif
	});
}

#if defined(_WIN32)
using sock_t = SOCKET;
bool sock_invalid(sock_t s) { return s == INVALID_SOCKET; }
void sock_close(sock_t s)
{
	if (!sock_invalid(s))
		::closesocket(s);
}
#else
using sock_t = int;
bool sock_invalid(sock_t s) { return s < 0; }
void sock_close(sock_t s)
{
	if (!sock_invalid(s))
		::close(s);
}
#endif

bool read_exact(sock_t s, void *buf, std::size_t n, std::string &err)
{
	auto *p = static_cast<unsigned char *>(buf);
	std::size_t got = 0;
	while (got < n) {
#if defined(_WIN32)
		int r = ::recv(s, reinterpret_cast<char *>(p + got), static_cast<int>(n - got), 0);
		if (r == SOCKET_ERROR || r == 0) {
			err = "socket closed before payload complete";
			return false;
		}
#else
		ssize_t r = ::recv(s, p + got, n - got, 0);
		if (r <= 0) {
			err = "socket closed before payload complete";
			return false;
		}
#endif
		got += static_cast<std::size_t>(r);
	}
	return true;
}

bool decode_required_b64(std::string_view json, char const *key, std::string &out, std::string &err)
{
	auto const v = coinline_json_string_field(json, key);
	if (!v) {
		err = std::string("missing field: ") + key;
		return false;
	}
	auto dec = coinline_base64_decode_string(*v);
	if (!dec) {
		err = std::string("invalid base64: ") + key;
		return false;
	}
	out = std::move(*dec);
	return true;
}

} // namespace

bool evidence_export_parse_wire_json(std::string const &json, millennium_evidence_bundle_params &out, std::string &err)
{
	err.clear();
	out = millennium_evidence_bundle_params{};
	if (!decode_required_b64(json, "scenario_json_b64", out.scenario_json, err))
		return false;
	if (!decode_required_b64(json, "boot_trace_jsonl_b64", out.boot_trace_jsonl_body, err))
		return false;
	if (!decode_required_b64(json, "vfd_final_json_b64", out.vfd_final_json, err))
		return false;

	if (auto sid = coinline_json_string_field(json, "scenario_id"))
		out.scenario_id = *sid;
	if (auto bp = coinline_json_string_field(json, "board_profile_relpath"))
		out.board_profile_relpath = *bp;
	if (auto sha = coinline_json_string_field(json, "firmware_sha256"))
		out.firmware_sha256 = *sha;
	std::uint64_t sz = 0;
	if (!coinline_json_uint64_field(json, "firmware_size", sz)) {
		err = "missing or invalid firmware_size";
		return false;
	}
	out.firmware_size = sz;

	bool det = false;
	if (!coinline_json_bool_field(json, "deterministic_timestamps", det)) {
		err = "missing or invalid deterministic_timestamps";
		return false;
	}
	out.deterministic_timestamps = det;

	if (auto ts0 = coinline_json_string_field(json, "ts_start"))
		out.ts_start = *ts0;
	if (auto ts1 = coinline_json_string_field(json, "ts_end"))
		out.ts_end = *ts1;

	if (auto fp = coinline_json_string_field(json, "firmware_path"))
		out.firmware_path = *fp;
	else
		out.firmware_path = "emulator";

	if (auto me = coinline_json_string_field(json, "mame_executable"))
		out.mame_executable = *me;
	bool expect_audio = false;
	if (coinline_json_bool_field(json, "expect_audio_traces", expect_audio))
		out.expect_audio_traces = expect_audio;
	if (auto am = coinline_json_string_field(json, "audio_milestones_claimed_json"))
		out.audio_milestones_claimed_json = *am;
	if (auto th = coinline_json_string_field(json, "audio_trace_sha256_json"))
		out.audio_trace_sha256_json = *th;

	auto decode_opt_b64 = [&](char const *key, std::string &dest) {
		if (auto raw = coinline_json_string_field(json, key)) {
			auto dec = coinline_base64_decode_string(*raw);
			if (!dec) {
				err = std::string("invalid base64: ") + key;
				return false;
			}
			dest = std::move(*dec);
		}
		return true;
	};
	if (!decode_opt_b64("audio_trace_jsonl_b64", out.audio_trace_jsonl_body))
		return false;
	if (!decode_opt_b64("voiceware_trace_jsonl_b64", out.voiceware_trace_jsonl_body))
		return false;
	if (!decode_opt_b64("supervision_trace_jsonl_b64", out.supervision_trace_jsonl_body))
		return false;
	if (!decode_opt_b64("alerter_trace_jsonl_b64", out.alerter_trace_jsonl_body))
		return false;
	if (!decode_opt_b64("audio_state_final_json_b64", out.audio_state_final_json_body))
		return false;
	if (!decode_opt_b64("io_trace_jsonl_b64", out.io_trace_jsonl_body))
		return false;

	out.result_milestone = "M10";
	out.result_status = "pass";
	return true;
}

bool evidence_bundle_export_from_run_directory(std::filesystem::path const &run_dir, std::filesystem::path const &out_dir,
	std::string const &scenario_id_override, std::string &err)
{
	err.clear();
	millennium_evidence_bundle_params p{};
	if (!millennium_evidence_bundle_fill_from_run_directory(run_dir, p, err))
		return false;
	if (!scenario_id_override.empty())
		p.scenario_id = scenario_id_override;
	std::filesystem::remove_all(out_dir);
	return millennium_evidence_bundle_write(out_dir, p, err);
}

bool evidence_export_recv_payload(std::string const &host, std::string const &port, std::string &payload_out, std::string &err)
{
	err.clear();
	payload_out.clear();
	net_init_once();

	int port_num = 0;
	try {
		port_num = std::stoi(port);
	} catch (...) {
		err = "invalid TCP port";
		return false;
	}
	if (port_num < 1 || port_num > 65535) {
		err = "invalid TCP port range";
		return false;
	}

#if defined(_WIN32)
	sock_t s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#else
	sock_t s = ::socket(AF_INET, SOCK_STREAM, 0);
#endif
	if (sock_invalid(s)) {
		err = "socket() failed";
		return false;
	}
	sockaddr_in a{};
	a.sin_family = AF_INET;
	a.sin_port = htons(static_cast<std::uint16_t>(port_num));
	if (::inet_pton(AF_INET, host.c_str(), &a.sin_addr) != 1) {
		err = "invalid host (IPv4 only)";
		sock_close(s);
		return false;
	}
	if (::connect(s, reinterpret_cast<sockaddr *>(&a), sizeof(a)) != 0) {
		err = "connect failed";
		sock_close(s);
		return false;
	}

	unsigned char lenbuf[4]{};
	if (!read_exact(s, lenbuf, 4, err)) {
		sock_close(s);
		return false;
	}
	std::uint32_t const n = (std::uint32_t(lenbuf[0]) << 24) | (std::uint32_t(lenbuf[1]) << 16) | (std::uint32_t(lenbuf[2]) << 8)
				| std::uint32_t(lenbuf[3]);
	if (n > 64U * 1024U * 1024U) {
		err = "payload too large";
		sock_close(s);
		return false;
	}
	payload_out.assign(n, '\0');
	if (n > 0 && !read_exact(s, payload_out.data(), n, err)) {
		sock_close(s);
		return false;
	}
	sock_close(s);
	while (!payload_out.empty() && std::isspace(static_cast<unsigned char>(payload_out.back())))
		payload_out.pop_back();
	while (!payload_out.empty() && std::isspace(static_cast<unsigned char>(payload_out.front())))
		payload_out.erase(payload_out.begin());
	return true;
}

bool evidence_bundle_export_run(std::string const &emulator_host_port, std::filesystem::path const &out_dir,
	std::string const &scenario_id_override, std::string &err)
{
	err.clear();
	auto const colon = emulator_host_port.rfind(':');
	if (colon == std::string::npos || colon == 0 || colon + 1 >= emulator_host_port.size()) {
		err = "expected --emulator host:port (IPv4)";
		return false;
	}
	std::string const host = emulator_host_port.substr(0, colon);
	std::string const port = emulator_host_port.substr(colon + 1);

	std::string payload;
	if (!evidence_export_recv_payload(host, port, payload, err))
		return false;

	millennium_evidence_bundle_params p{};
	if (!evidence_export_parse_wire_json(payload, p, err))
		return false;
	if (!scenario_id_override.empty())
		p.scenario_id = scenario_id_override;

	std::filesystem::remove_all(out_dir);
	return millennium_evidence_bundle_write(out_dir, p, err);
}
