// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_pcd3349a.h"
#include "millennium_debug.h"

#include <array>
#include <cstdlib>
#include <cstdio>
#include <fstream>

namespace {

constexpr std::size_t k_tp_rom_size = 4096U;

std::array<std::uint8_t, k_tp_rom_size> make_placeholder_tp_rom()
{
	// as authority and execute this deterministic placeholder image through the 8048 contract for parity testing.
	std::array<std::uint8_t, k_tp_rom_size> rom{};
	for (std::size_t i = 0; i < rom.size(); ++i)
		rom[i] = static_cast<std::uint8_t>((i * 13U + 0x72U) & 0xffU);
	// Seed obvious behavioral op points used by the minimalist core.
	rom[0] = 0x01U;
	rom[1] = 0x02U;
	rom[2] = 0x03U;
	rom[3] = 0x00U;
	return rom;
}

bool load_rom_file(std::filesystem::path const &path, std::array<std::uint8_t, k_tp_rom_size> &out)
{
	std::ifstream f(path, std::ios::binary);
	if (!f.good())
		return false;
	f.read(reinterpret_cast<char *>(out.data()), static_cast<std::streamsize>(out.size()));
	std::streamsize const got = f.gcount();
	if (got <= 0)
		return false;
	for (std::size_t i = static_cast<std::size_t>(got); i < out.size(); ++i)
		out[i] = 0x00U;
	return true;
}

} // namespace

void millennium_pcd3349a::set_trace_paths(std::filesystem::path runtime, std::filesystem::path port, std::filesystem::path keypad,
	std::filesystem::path tone, std::filesystem::path cp_protocol)
{
	m_runtime_trace_path = std::move(runtime);
	m_port_trace_path = std::move(port);
	m_keypad_trace_path = std::move(keypad);
	m_tone_trace_path = std::move(tone);
	m_cp_protocol_trace_path = std::move(cp_protocol);
}

void millennium_pcd3349a::append_trace(std::filesystem::path const &path, char const *event, std::uint64_t cp_cycle, std::uint8_t value) const
{
	if (path.empty())
		return;
	char b[320];
	std::snprintf(b, sizeof(b),
		"{\"backend\":\"pcd3349a_8048\",\"tp_xtal_hz\":%u,\"tp_cycle\":%llu,\"cp_cycle\":%llu,"
		"\"pc\":\"0x%04X\",\"event\":\"%s\",\"value\":\"0x%02X\"}",
		unsigned(m_tp_xtal_hz), static_cast<unsigned long long>(m_core.cycles()),
		static_cast<unsigned long long>(cp_cycle), unsigned(m_core.debug_pc() & 0xffffU), event ? event : "",
		unsigned(value));
	millennium_boot_trace_append_line(path, b);
}

void millennium_pcd3349a::reset(bool onhook_initial)
{
	static std::array<std::uint8_t, k_tp_rom_size> rom = make_placeholder_tp_rom();
	if (!m_rom_loaded) {
		char const *const env = std::getenv("COINLINE_TP8048_ROM");
		if (env && *env && load_rom_file(std::filesystem::path(env), rom))
			m_rom_source = std::string("rom:") + env;
		else if (load_rom_file(std::filesystem::path("firmware/telephony_subprocessor.rom"), rom))
			m_rom_source = "rom:firmware/telephony_subprocessor.rom";
		else
			m_rom_source = "placeholder";
		m_rom_loaded = true;
	}
	m_core.load_program_rom(rom.data(), rom.size());
	m_core.reset();
	m_tp.load_firmware_rom(rom.data(), rom.size());
	m_tp.reset();
	// Idle on-hook P1 sample so behavioural firmware can emit POWER_ON_RESET vs POWER_ON_ACK correctly
	// without consuming main-CPU cycle coupling (avoid run_until_cp_cycle before Z180 timebase exists).
	{
		coinline::tp8048::tp_input_snapshot snap{};
		snap.keymatrix = 0U;
		snap.linectrl = 0x07U;
		m_tp.set_input_snapshot(snap);
		m_tp.step_tp_cycles_raw(400000ULL);
	}
	m_last_keymatrix = 0U;
	m_last_linectrl = 0U;
	m_last_softkeys = 0U;
	m_hook_last_transition_cycle = 0ULL;
	m_hook_onhook = onhook_initial;
	append_trace(m_runtime_trace_path, "reset", 0ULL, 0U);
	append_trace(m_runtime_trace_path, m_rom_source.c_str(), 0ULL, 0U);
}

millennium_pcd3349a::front_panel_result millennium_pcd3349a::process_front_panel(std::uint32_t keymatrix,
	std::uint32_t linectrl, std::uint32_t softkeys_raw, std::uint8_t secmask, bool oos_visible, bool voice_active,
	millennium_terminal21_user_io_profile profile, std::uint64_t cycle, std::uint64_t hz)
{
	(void)hz;
	front_panel_result r{};
	coinline::tp8048::tp_input_snapshot snap{};
	snap.keymatrix = keymatrix;
	snap.linectrl = static_cast<std::uint8_t>(linectrl & 0xffU);
	snap.softkeys = (profile == millennium_terminal21_user_io_profile::vfd_11line_softkeys)
		? static_cast<std::uint16_t>(softkeys_raw & 0x0fffU)
		: 0U;
	snap.secmask = secmask;
	snap.oos_visible = oos_visible;
	snap.voice_active = voice_active;
	snap.cp_cycle = cycle;
	snap.terminal_21_profile = profile;
	m_tp.set_input_snapshot(snap);
	m_tp.run_until_cp_cycle(cycle, m_cp_hz);
	append_trace(m_runtime_trace_path, "input_snapshot", cycle, static_cast<std::uint8_t>(linectrl & 0xffU));
	append_trace(m_port_trace_path, "port_sample_p1", cycle, static_cast<std::uint8_t>(keymatrix & 0xffU));
	while (m_tp.has_ui_event())
		r.tp_events.push_back(m_tp.pop_ui_event());
	for (std::uint8_t ev : r.tp_events) {
		if (ev == 0x6EU) {
			r.hook_changed = true;
			r.hook_onhook = false;
			m_hook_onhook = false;
		} else if (ev == 0x6CU) {
			r.hook_changed = true;
			r.hook_onhook = true;
			m_hook_onhook = true;
		}
	}
	for (std::uint8_t ev : r.tp_events)
		append_trace(m_keypad_trace_path, "tp_key_event", cycle, ev);
	return r;
}

std::vector<std::uint8_t> millennium_pcd3349a::handle_cp_to_tp_byte(std::uint8_t byte, bool hook_onhook_stable)
{
	(void)hook_onhook_stable;
	m_tp.receive_cp_byte(byte);
	append_trace(m_cp_protocol_trace_path, "cp_to_tp_byte", m_core.cycles(), byte);
	std::vector<std::uint8_t> out;
	while (m_tp.has_tx_byte()) {
		std::uint8_t const v = m_tp.pop_tx_byte();
		out.push_back(v);
		append_trace(m_cp_protocol_trace_path, "tp_to_cp_byte", m_core.cycles(), v);
	}
	return out;
}

millennium_pcd3349a::tone_mode millennium_pcd3349a::compute_tone_mode(bool off_hook, bool rx_open, bool voice_active, bool oos_mode) const noexcept
{
	auto const obs = m_tp.observable();
	switch (obs.tone) {
	case coinline::tp8048::tp_tone_mode::nis:
		append_trace(m_tone_trace_path, "tone_mode", m_core.cycles(), 0x02U);
		return tone_mode::nis;
	case coinline::tp8048::tp_tone_mode::dialtone:
		append_trace(m_tone_trace_path, "tone_mode", m_core.cycles(), 0x01U);
		return tone_mode::dialtone;
	case coinline::tp8048::tp_tone_mode::dtmf:
		append_trace(m_tone_trace_path, "tone_mode", m_core.cycles(), 0x03U);
		return off_hook && rx_open ? tone_mode::dialtone : tone_mode::none;
	default:
		break;
	}
	// Fallback when observable has not run update_tone_mode yet: same policy — OOS NIS until CP seizes hook relay.
	if (off_hook && rx_open && oos_mode && voice_active == false && !m_tp.relay_cp_seized())
		return tone_mode::nis;
	return tone_mode::none;
}
