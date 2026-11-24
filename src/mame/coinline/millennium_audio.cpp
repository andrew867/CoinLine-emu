// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_audio.h"

#include "millennium_audio_trace.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

void *coinline_mic_open();
void coinline_mic_close(void *mic);
float coinline_mic_read(void *mic);

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
	, device_sound_interface(mconfig, *this)
	, m_maincpu(*this, "^maincpu")
{
}

void millennium_audio_device::device_start()
{
	m_model.set_cpu_clock_hz(static_cast<double>(m_maincpu->unscaled_clock()));
	m_stream = stream_alloc(0, 1, 48000);
	m_model.prepare_high_fast_ringer(48000);
	save_item(NAME(m_alerter_audible));
	open_microphone();
}

void millennium_audio_device::set_external_ringer(bool on) noexcept
{
	if (on && !m_external_ringer)
		m_model.reset_high_fast_ringer();
	m_external_ringer = on;
}

void millennium_audio_device::set_hybrid_gates(bool tx_muted, bool rx_muted, bool sidetone_suppressed) noexcept
{
	if (m_stream)
		m_stream->update();
	m_tx_muted = tx_muted;
	m_sidetone_suppressed = sidetone_suppressed;
	m_model.set_receive_muted(rx_muted);
}

void millennium_audio_device::set_handset_gain(float gain)
{
	if (m_stream)
		m_stream->update();
	m_model.set_receive_gain(gain);
}

void millennium_audio_device::device_stop()
{
	close_microphone();
}

void millennium_audio_device::open_microphone()
{
	if (m_mic == nullptr)
		m_mic = coinline_mic_open();
}

void millennium_audio_device::close_microphone()
{
	coinline_mic_close(m_mic);
	m_mic = nullptr;
}

void millennium_audio_device::sound_stream_update(sound_stream &stream)
{
	double const t0 = machine().time().as_double();
	double const step = 1.0 / 48000.0;
	for (int i = 0; i < stream.samples(); ++i) {
		bool const line_tick = ++m_pcm_div == 6U;
		if (line_tick) {
			m_pcm_div = 0;
			m_mic_sample = std::clamp(coinline_mic_read(m_mic), -1.f, 1.f);
		}
		float const transmit = m_tx_muted ? 0.f : m_mic_sample;
		double const t = t0 + step * double(i);
		float s = m_alerter_audible ? m_model.alerter_speaker_sample(t) : m_model.handset_speaker_sample(t);
		if (m_alerter_audible && m_external_ringer)
			s += m_model.pull_high_fast_ringer();
		else if (!m_alerter_audible && !m_sidetone_suppressed)
			s += 0.12f * transmit;
		if (line_tick && m_pcm_tap) {
			std::int16_t line = 0;
			bool have = false;
			m_pcm_tap(m_pcm_ctx, static_cast<std::int16_t>(transmit * 32767.f), line, have);
			if (have && !m_alerter_audible)
				inject_handset_sample(static_cast<float>(line) / 32767.f);
		}
		if (m_alerter_audible && m_cadence_kind != millennium_alerter_cadence_kind::none && (m_cadence_edge_next & 1U) != 0U)
			s += 0.35f * static_cast<float>(std::sin(2.0 * 3.14159265 * 1000.0 * t));
		if (s > 1.f)
			s = 1.f;
		else if (s < -1.f)
			s = -1.f;
		int const pcm = int(s * 32767.f);
		stream.put_int(0, i, pcm, 32768);
	}
}

void millennium_audio_device::device_reset()
{
	device_t::device_reset();
	cancel_cadence();
	m_external_ringer = false;
	m_tx_muted = false;
	m_sidetone_suppressed = false;
	m_mic_sample = 0.f;
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
	m_model.write_dtmf_digit(ascii_digit, duration_ms, machine().time().as_double());
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
