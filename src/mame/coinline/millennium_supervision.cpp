// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_supervision.h"

#include "millennium_audio_trace.h"

DEFINE_DEVICE_TYPE(MILLENNIUM_SUPERVISION, millennium_supervision_device, "millennium_supervision",
	"CoinLine disconnect supervision")

namespace {

char const *disconnect_kind_cstr(coinline::supervision::disconnect_kind k)
{
	using coinline::supervision::disconnect_kind;
	switch (k) {
	case disconnect_kind::normal_disconnect:
		return "normal_disconnect";
	case disconnect_kind::cpc:
		return "cpc";
	case disconnect_kind::timeout:
		return "timeout";
	case disconnect_kind::fault:
		return "fault";
	default:
		return "none";
	}
}

} // namespace

millennium_supervision_device::millennium_supervision_device(machine_config const &mconfig, char const *tag,
	device_t *owner, u32 clock)
	: device_t(mconfig, MILLENNIUM_SUPERVISION, tag, owner, clock)
	, m_maincpu(*this, "^maincpu")
{
}

void millennium_supervision_device::device_start() {}

void millennium_supervision_device::device_reset()
{
	device_t::device_reset();
	m_fsm.reset();
}

std::uint64_t millennium_supervision_device::emulated_ns() const
{
	double const sec = machine().time().as_double();
	return static_cast<std::uint64_t>(sec * 1e9);
}

void millennium_supervision_device::emit_fsm_events(
	std::vector<coinline::supervision::supervision_fsm_event> const &events)
{
	for (auto const &e : events) {
		switch (e.kind) {
		case coinline::supervision::supervision_fsm_event::kind::supervision_status_code: {
			std::string const j = millennium_audio_trace_supervision_status_code_json(emulated_ns(), e.cycle, e.pc,
				e.status_code);
			millennium_audio_trace_emit_supervision_row(j);
			break;
		}
		case coinline::supervision::supervision_fsm_event::kind::disconnect_event: {
			std::string const j = millennium_audio_trace_disconnect_event_json(emulated_ns(), e.cycle, e.pc,
				disconnect_kind_cstr(e.disconnect));
			millennium_audio_trace_emit_supervision_row(j);
			break;
		}
		case coinline::supervision::supervision_fsm_event::kind::supervision_timeout: {
			std::string const j =
				millennium_audio_trace_supervision_timeout_json(emulated_ns(), e.cycle, e.pc);
			millennium_audio_trace_emit_supervision_row(j);
			break;
		}
		}
	}
}

void millennium_supervision_device::on_processor_status_byte(std::uint8_t code, std::uint64_t cpu_cycle,
	std::uint16_t pc)
{
	emit_fsm_events(m_fsm.step_status(code, cpu_cycle, pc));
}

void millennium_supervision_device::on_modem_carrier(bool carrier_up, std::uint64_t cpu_cycle,
	std::uint16_t pc)
{
	emit_fsm_events(m_fsm.step_carrier(carrier_up, cpu_cycle, pc));
}

void millennium_supervision_device::on_watchdog_fired(std::uint64_t cpu_cycle, std::uint16_t pc)
{
	emit_fsm_events(m_fsm.step_watchdog_fired(cpu_cycle, pc));
}
