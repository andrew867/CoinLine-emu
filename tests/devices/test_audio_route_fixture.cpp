// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_audio_route_apply.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

std::string slurp(std::filesystem::path const &p)
{
	std::ifstream f(p);
	if (!f)
		return {};
	std::ostringstream ss;
	ss << f.rdbuf();
	return ss.str();
}

bool contains(std::string const &hay, std::string const &needle)
{
	return hay.find(needle) != std::string::npos;
}

} // namespace

int main()
{
	std::filesystem::path const root(COINLINE_EMU_SOURCE_DIR);

	coinline::audio_route::composite_state st;
	coinline::audio_route::reset_to_idle_fixture(st);
	std::string route, mute;
	coinline::audio_route::snapshot_route_string(st, route);
	coinline::audio_route::snapshot_mute_json(st, mute);
	std::string const idle_fixture = slurp(root / "fixtures/audio/audio-route-idle.json");
	if (idle_fixture.empty()) {
		std::cerr << "missing audio-route-idle.json\n";
		return 1;
	}
	if (route != "idle" || mute != "{\"mic\":false,\"earpiece\":false}") {
		std::cerr << "idle snapshot mismatch\n";
		return 1;
	}
	if (!contains(idle_fixture, "route_state") || !contains(idle_fixture, "idle")) {
		std::cerr << "fixture idle string unexpected\n";
		return 1;
	}

	char const *lb = nullptr;
	char const *ef = nullptr;
	unsigned const seq[] = { 0x40, 0x42, 0x27, 0x29 };
	for (unsigned b : seq) {
		if (!coinline::audio_route::lookup_command(static_cast<std::uint8_t>(b), lb, ef))
			return 1;
		std::string notes;
		if (!coinline::audio_route::apply_effect(st, ef, &notes))
			return 1;
	}
	coinline::audio_route::snapshot_route_string(st, route);
	coinline::audio_route::snapshot_mute_json(st, mute);
	if (route != "call_connected") {
		std::cerr << "expected call_connected got " << route << '\n';
		return 1;
	}
	if (mute != "{\"mic\":true,\"earpiece\":true}") {
		std::cerr << "expected muted tuple got " << mute << '\n';
		return 1;
	}

	std::cout << "audio route Class A replay ok\n";
	return 0;
}
