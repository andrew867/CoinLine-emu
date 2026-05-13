// SPDX-License-Identifier: GPL-2.0-or-later

#include "evidence_export_client.h"

#include "millennium_evidence_bundle.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <string>
#include <thread>

#ifndef COINLINE_EMU_SOURCE_DIR
#error COINLINE_EMU_SOURCE_DIR
#endif

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

#if defined(_WIN32)
using sock_t = SOCKET;
bool bad_sock(sock_t s) { return s == INVALID_SOCKET; }
void close_sock(sock_t s)
{
	if (!bad_sock(s))
		::closesocket(s);
}
#else
using sock_t = int;
bool bad_sock(sock_t s) { return s < 0; }
void close_sock(sock_t s)
{
	if (!bad_sock(s))
		::close(s);
}
#endif

bool send_all_raw(sock_t s, void const *buf, std::size_t n)
{
	auto const *p = static_cast<unsigned char const *>(buf);
	std::size_t sent = 0;
	while (sent < n) {
#if defined(_WIN32)
		int r = ::send(s, reinterpret_cast<char const *>(p + sent), static_cast<int>(n - sent), 0);
		if (r == SOCKET_ERROR || r == 0)
			return false;
#else
		ssize_t r = ::send(s, p + sent, n - sent, 0);
		if (r <= 0)
			return false;
#endif
		sent += static_cast<std::size_t>(r);
	}
	return true;
}

void mock_server_thread(std::atomic<std::uint16_t> *port_out, std::atomic<bool> *ok, std::string const &payload)
{
#if defined(_WIN32)
	WSADATA w{};
	if (::WSAStartup(MAKEWORD(2, 2), &w) != 0) {
		ok->store(false);
		return;
	}
#endif
	sock_t ls = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (bad_sock(ls)) {
		ok->store(false);
		return;
	}
	int one = 1;
	(void)::setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char const *>(&one), sizeof(one));
	sockaddr_in a{};
	a.sin_family = AF_INET;
	a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	a.sin_port = 0;
	if (::bind(ls, reinterpret_cast<sockaddr *>(&a), sizeof(a)) != 0) {
		close_sock(ls);
		ok->store(false);
		return;
	}
	int alen = sizeof(a);
	if (::getsockname(ls, reinterpret_cast<sockaddr *>(&a), &alen) != 0) {
		close_sock(ls);
		ok->store(false);
		return;
	}
	if (::listen(ls, 1) != 0) {
		close_sock(ls);
		ok->store(false);
		return;
	}
	port_out->store(ntohs(a.sin_port), std::memory_order_relaxed);
	sockaddr_in peer{};
	int plen = sizeof(peer);
	sock_t c = ::accept(ls, reinterpret_cast<sockaddr *>(&peer), &plen);
	close_sock(ls);
	if (bad_sock(c)) {
		ok->store(false);
		return;
	}
	std::uint32_t const n = static_cast<std::uint32_t>(payload.size());
	unsigned char hdr[4] = {static_cast<unsigned char>((n >> 24) & 0xFF), static_cast<unsigned char>((n >> 16) & 0xFF),
		static_cast<unsigned char>((n >> 8) & 0xFF), static_cast<unsigned char>(n & 0xFF)};
	if (!send_all_raw(c, hdr, 4) || (n > 0 && !send_all_raw(c, payload.data(), n))) {
		close_sock(c);
		ok->store(false);
		return;
	}
	close_sock(c);
}

} // namespace

int main()
{
	std::string const fixture = std::string(COINLINE_EMU_SOURCE_DIR) + "/tools/evidence-bundle-export/tests/fixtures/wire-export-payload.json";
	std::ifstream fin(fixture);
	if (!fin)
		return 1;
	std::ostringstream ss;
	ss << fin.rdbuf();
	std::string const payload = ss.str();

	millennium_evidence_bundle_params p{};
	std::string err;
	if (!evidence_export_parse_wire_json(payload, p, err))
		return 1;
	if (p.scenario_id != "wire-test")
		return 1;
	if (p.firmware_path != "emulator")
		return 1;

	std::filesystem::path const tmp = std::filesystem::temp_directory_path() / "coinline_export_parse_test";
	std::filesystem::remove_all(tmp);
	if (!millennium_evidence_bundle_write(tmp, p, err))
		return 1;
	if (!std::filesystem::exists(tmp / "manifest.json") || !std::filesystem::exists(tmp / "vfd" / "final.json"))
		return 1;
	std::filesystem::remove_all(tmp);

	std::atomic<std::uint16_t> port{0};
	std::atomic<bool> srv_ok{true};
	std::thread t(mock_server_thread, &port, &srv_ok, payload);
	auto const t0 = std::chrono::steady_clock::now();
	while (port.load(std::memory_order_relaxed) == 0 && std::chrono::steady_clock::now() - t0 < std::chrono::seconds(5))
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	if (port.load(std::memory_order_relaxed) == 0) {
		t.join();
		return 1;
	}

	std::filesystem::path const tmp2 = std::filesystem::temp_directory_path() / "coinline_export_socket_test";
	std::filesystem::remove_all(tmp2);
	if (!evidence_bundle_export_run(std::string("127.0.0.1:") + std::to_string(port.load()), tmp2, "", err)) {
		t.join();
		return 1;
	}
	t.join();
	if (!srv_ok.load())
		return 1;
	if (!std::filesystem::exists(tmp2 / "scenario.json"))
		return 1;
	std::filesystem::remove_all(tmp2);

	if (evidence_export_parse_wire_json("{bad", p, err))
		return 1;

	/* Audio bundle: wire payload → bundle contains audio/audio-trace.jsonl */
	{
		std::string const fixture_audio =
			std::string(COINLINE_EMU_SOURCE_DIR) + "/tools/evidence-bundle-export/tests/fixtures/wire-audio-payload.json";
		std::ifstream fa(fixture_audio);
		if (!fa)
			return 2;
		std::ostringstream sa;
		sa << fa.rdbuf();
		millennium_evidence_bundle_params pa{};
		if (!evidence_export_parse_wire_json(sa.str(), pa, err))
			return 2;
		if (!pa.expect_audio_traces)
			return 2;
		std::filesystem::path const tmpa = std::filesystem::temp_directory_path() / "coinline_export_audio_test";
		std::filesystem::remove_all(tmpa);
		if (!millennium_evidence_bundle_write(tmpa, pa, err))
			return 2;
		if (!std::filesystem::exists(tmpa / "audio" / "audio-trace.jsonl"))
			return 2;
		std::string mans;
		{
			std::ifstream mf(tmpa / "manifest.json");
			std::ostringstream ms;
			ms << mf.rdbuf();
			mans = ms.str();
		} // close stream before remove_all (Windows locks open files)
		if (mans.find("mame_executable") == std::string::npos || mans.find("coinline-mame.exe") == std::string::npos)
			return 2;
		if (mans.find("firmware_sha256") == std::string::npos || mans.find("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb") == std::string::npos)
			return 2;
		std::filesystem::remove_all(tmpa);
	}

	/* expect_audio_traces without mame_executable → fail */
	{
		millennium_evidence_bundle_params pb{};
		pb.expect_audio_traces = true;
		pb.firmware_sha256.assign(64, 'c');
		pb.scenario_id = "bad-audio";
		pb.scenario_json = "{}";
		pb.boot_trace_jsonl_body = "{\"milestone\":\"M0\"}\n";
		pb.vfd_final_json = "{\"variant\":\"2line\",\"rows\":2,\"columns\":20,\"text\":[\"                    \",\"                    \"],\"raw\":\"\"}\n";
		if (millennium_evidence_bundle_write(std::filesystem::temp_directory_path() / "coinline_should_fail", pb, err))
			return 3;
	}

	/* Run-directory import */
	{
		std::filesystem::path const rd =
			std::filesystem::temp_directory_path() / "coinline_run_import_src";
		std::filesystem::remove_all(rd);
		std::filesystem::create_directories(rd);
		std::string errw;
		auto wf = [&](char const *name, char const *body) {
			std::ofstream o(rd / name);
			o << body;
		};
		wf("boot-trace.jsonl", "{\"milestone\":\"M0\"}\n");
		wf("audio-trace.jsonl",
			"{\"schema_version\":\"coinline.audio_trace/v1\",\"device\":\"audio\",\"event_type\":\"run_import\"}\n");
		wf("voiceware-trace.jsonl",
			"{\"schema_version\":\"coinline.audio_trace/v1\",\"device\":\"voiceware\",\"event_type\":\"run_import\"}\n");
		wf("io-trace.jsonl", "{\"port\":\"0x61\"}\n");
		{
			std::ofstream summ(rd / "evidence-summary.json");
			summ << "{\"emulator_executable\":\"D:/bin/coinline-mame.exe\",\"firmware_sha256\":"
				<< "\"dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd\"}\n";
		} // close before export reads evidence-summary.json (Windows file sharing)
		std::filesystem::path const outb = std::filesystem::temp_directory_path() / "coinline_run_import_bundle";
		std::filesystem::remove_all(outb);
		if (!evidence_bundle_export_from_run_directory(rd, outb, "run-import-test", errw))
			return 4;
		if (!std::filesystem::exists(outb / "audio" / "audio-trace.jsonl"))
			return 4;
		std::filesystem::remove_all(rd);
		std::filesystem::remove_all(outb);
	}

	return 0;
}
