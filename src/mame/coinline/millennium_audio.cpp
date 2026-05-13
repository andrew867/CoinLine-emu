// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_audio.h"

#include "millennium_audio_trace.h"

DEFINE_DEVICE_TYPE(MILLENNIUM_AUDIO, millennium_audio_device, "millennium_audio", "CoinLine Millennium alerter audio")

namespace {

char const *cadence_kind_label(millennium_alerter_cadence_kind k)
{
	switch (k) {
	case millennium_alerter_cadence_kind::service_beep:
		return "service_beep";
	case millennium_alerter_cadence_kind::error_beep:
		return "error_beep";
	case millennium_alerter_cadence_kind::user_prompt_tone:
		return "user_prompt_tone";
	default:
		return "none";
	}
}

} // namespace

millennium_audio_device::millennium_audio_device(machine_config const &mconfig, char const *tag, device_t *owner,
	u32 clock)
	: device_t(mconfig, MILLENNIUM_AUDIO, tag, owner, clock)
	, m_maincpu(*this, "^maincpu")
{
}

void millennium_audio_device::device_start()
{
	m_model.set_cpu_clock_hz(static_cast<double>(m_maincpu->unscaled_clock()));
}

void millennium_audio_device::device_reset()
{
	device_t::device_reset();
	cancel_cadence();
	m_model.reset();
	emit_alerter_ready();
}

void millennium_audio_device::apply_config(millennium_alerter_board_config const &cfg)
{
	m_model.configure(cfg);
	m_model.reset();
}

std::uint64_t millennium_audio_device::emulated_ns() const
{
	double const sec = machine().time().as_double();
	return static_cast<std::uint64_t>(sec * 1e9);
}

void millennium_audio_device::emit_alerter_ready()
{
	std::string const j = millennium_audio_trace_alerter_ready_json(emulated_ns(), 0ULL, 0U);
	millennium_audio_trace_emit_alerter_row(j);
}

void millennium_audio_device::emit_alerter_gpio(std::uint16_t port, std::uint8_t data, std::uint64_t cpu_cycle,
	std::uint16_t pc)
{
	std::string const j =
		millennium_audio_trace_alerter_gpio_json(emulated_ns(), cpu_cycle, pc, port, data);
	millennium_audio_trace_emit_alerter_row(j);
}

void millennium_audio_device::cancel_cadence()
{
	++m_cadence_seq;
	m_cadence_kind = millennium_alerter_cadence_kind::none;
	m_cadence_edge_next = 0;
	m_cadence_edge_count = 0;
}

void millennium_audio_device::start_cadence(millennium_alerter_cadence_kind kind, std::uint64_t cpu_cycle,
	std::uint16_t pc)
{
	cancel_cadence();
	m_cadence_kind = kind;
	m_cadence_cycle_anchor = cpu_cycle;
	m_cadence_pc_anchor = pc;
	m_cadence_edge_count = millennium_audio_model::cadence_edge_count(kind);
	for (unsigned i = 0; i < m_cadence_edge_count &&
			    i < unsigned(sizeof(m_cadence_edges_storage) / sizeof(m_cadence_edges_storage[0]));
	     ++i)
		m_cadence_edges_storage[i] =
			static_cast<u16>(millennium_audio_model::cadence_edge_ms(kind, i));
	m_cadence_edge_next = 0;
	s32 const seq_param = static_cast<s32>(m_cadence_seq);
	machine().scheduler().timer_set(attotime::zero,
		timer_expired_delegate(FUNC(millennium_audio_device::cadence_edge_tick), this), seq_param);
}

TIMER_CALLBACK_MEMBER(millennium_audio_device::cadence_edge_tick)
{
	if (static_cast<unsigned>(param) != m_cadence_seq ||
	    m_cadence_kind == millennium_alerter_cadence_kind::none)
		return;
	if (m_cadence_edge_next >= m_cadence_edge_count) {
		m_cadence_kind = millennium_alerter_cadence_kind::none;
		return;
	}

	unsigned const idx = m_cadence_edge_next;
	bool const is_start = (idx % 2U) == 0U;
	unsigned const edge_ms = m_cadence_edges_storage[idx];
	char const *tcls = cadence_kind_label(m_cadence_kind);
	std::uint64_t const cy = m_maincpu->total_cycles();
	std::uint16_t const pc_now = static_cast<std::uint16_t>(m_maincpu->pc() & 0xffffU);

	std::string line = is_start ? millennium_audio_trace_alerter_tone_start_json(emulated_ns(), cy, pc_now, tcls,
						   edge_ms, idx)
				    : millennium_audio_trace_alerter_tone_end_json(emulated_ns(), cy, pc_now, tcls, edge_ms,
						  idx);
	millennium_audio_trace_emit_alerter_row(line);

	m_cadence_edge_next++;
	if (m_cadence_edge_next >= m_cadence_edge_count) {
		m_cadence_kind = millennium_alerter_cadence_kind::none;
		return;
	}

	unsigned const prev_ms = m_cadence_edges_storage[idx];
	unsigned const next_ms = m_cadence_edges_storage[m_cadence_edge_next];
	unsigned const delta = next_ms - prev_ms;
	machine().scheduler().timer_set(attotime::from_msec(delta),
		timer_expired_delegate(FUNC(millennium_audio_device::cadence_edge_tick), this), param);
}

void millennium_audio_device::write_dtmf_ascii(std::uint8_t ascii_digit, std::uint64_t cpu_cycle, std::uint16_t pc)
{
	emit_alerter_gpio(0x59, ascii_digit, cpu_cycle, pc);
}

void millennium_audio_device::write_tone_select(std::uint8_t data, std::uint64_t cpu_cycle, std::uint16_t pc)
{
	m_model.write_tone_select(data);
	emit_alerter_gpio(0x58, data, cpu_cycle, pc);

	millennium_alerter_board_config const &cfg = m_model.config();
	std::uint8_t const nib = data & 0x0fU;
	if (nib == (cfg.service_tone_nibble & 0x0fU))
		start_cadence(millennium_alerter_cadence_kind::service_beep, cpu_cycle, pc);
	else if (nib == (cfg.error_tone_nibble & 0x0fU))
		start_cadence(millennium_alerter_cadence_kind::error_beep, cpu_cycle, pc);
	else if (nib == (cfg.user_prompt_tone_nibble & 0x0fU))
		start_cadence(millennium_alerter_cadence_kind::user_prompt_tone, cpu_cycle, pc);
}

void millennium_audio_device::write_dtmf_digit(std::uint8_t ascii_digit, unsigned duration_ms, std::uint64_t cpu_cycle,
	std::uint16_t pc)
{
	m_model.write_dtmf_digit(ascii_digit, duration_ms);
	emit_alerter_gpio(0x5a, static_cast<std::uint8_t>(duration_ms & 0xffU), cpu_cycle, pc);
}

void millennium_audio_device::write_volume(std::uint8_t data, std::uint64_t cpu_cycle, std::uint16_t pc)
{
	m_model.write_volume(data);
	emit_alerter_gpio(0x5b, data, cpu_cycle, pc);
}

void millennium_audio_device::notify_mach_port_f(std::uint8_t prev_data, std::uint8_t next_data, std::uint64_t cpu_cycle,
	double cpu_hz)
{
	m_model.notify_mach_port_f(prev_data, next_data, cpu_cycle, cpu_hz);
	std::uint16_t const pc = static_cast<std::uint16_t>(m_maincpu->pc() & 0xffffU);
	emit_alerter_gpio(0xc3, next_data, cpu_cycle, pc);
}
