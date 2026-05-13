// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "emu.h"

#include "cpu/z180/z180.h"

#include "millennium_audio_model.h"

DECLARE_DEVICE_TYPE(MILLENNIUM_AUDIO, millennium_audio_device)

class millennium_audio_device : public device_t {
public:
	millennium_audio_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock = 0);

	void device_start() override ATTR_COLD;
	void device_reset() override;

	void apply_config(millennium_alerter_board_config const &cfg);

	void write_tone_select(std::uint8_t data, std::uint64_t cpu_cycle, std::uint16_t pc);
	void write_dtmf_ascii(std::uint8_t ascii_digit, std::uint64_t cpu_cycle, std::uint16_t pc);
	void write_dtmf_digit(std::uint8_t ascii_digit, unsigned duration_ms, std::uint64_t cpu_cycle, std::uint16_t pc);
	void write_volume(std::uint8_t data, std::uint64_t cpu_cycle, std::uint16_t pc);

	/// MACH port \c F (\c 0xC3): square-wave bit toggles + cadence gate — forwarded into \c millennium_audio_model.
	void notify_mach_port_f(std::uint8_t prev_data, std::uint8_t next_data, std::uint64_t cpu_cycle, double cpu_hz);

	void set_handset_loopback(bool on) noexcept { m_model.set_handset_loopback(on); }
	void inject_handset_sample(float s) noexcept { m_model.inject_handset_sample(s); }

	float output_sample(double t_sec) const { return m_model.output_sample(t_sec); }

	millennium_audio_model &model() noexcept { return m_model; }

private:
	void emit_alerter_ready();
	void emit_alerter_gpio(std::uint16_t port, std::uint8_t data, std::uint64_t cpu_cycle, std::uint16_t pc);
	void start_cadence(millennium_alerter_cadence_kind kind, std::uint64_t cpu_cycle, std::uint16_t pc);
	void cancel_cadence();
	std::uint64_t emulated_ns() const;

	TIMER_CALLBACK_MEMBER(cadence_edge_tick);

	required_device<z180_device> m_maincpu;
	millennium_audio_model m_model{};
	millennium_alerter_cadence_kind m_cadence_kind = millennium_alerter_cadence_kind::none;
	std::uint16_t m_cadence_edges_storage[8]{};
	unsigned m_cadence_edge_count = 0;
	unsigned m_cadence_edge_next = 0;
	std::uint64_t m_cadence_cycle_anchor = 0;
	std::uint16_t m_cadence_pc_anchor = 0;
	unsigned m_cadence_seq = 0;
};
