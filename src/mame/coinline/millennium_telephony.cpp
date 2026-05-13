// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_telephony.h"

#include "millennium_audio_route.h"
#include "millennium_audio_route_apply.h"
#include "millennium_audio_trace.h"
#include "millennium_supervision.h"

DEFINE_DEVICE_TYPE(MILLENNIUM_TELEPHONY, millennium_telephony_device, "millennium_telephony",
	"CoinLine telephony bridge")

millennium_telephony_device::millennium_telephony_device(machine_config const &mconfig, char const *tag,
	device_t *owner, u32 clock)
	: device_t(mconfig, MILLENNIUM_TELEPHONY, tag, owner, clock)
	, m_route(*this, "^:audroute")
	, m_supervision(*this, "^:supervision")
{
}

void millennium_telephony_device::device_start() {}

void millennium_telephony_device::device_reset()
{
	device_t::device_reset();
}

std::uint64_t millennium_telephony_device::emulated_ns() const
{
	double const sec = machine().time().as_double();
	return static_cast<std::uint64_t>(sec * 1e9);
}

void millennium_telephony_device::host_to_processor_byte(std::uint8_t data, std::uint64_t cpu_cycle,
	std::uint16_t pc)
{
	std::string const raw_line =
		millennium_audio_trace_telephony_raw_json(emulated_ns(), cpu_cycle, pc, data, "direction_host_to_processor");
	millennium_audio_trace_emit_telephony_row(raw_line);

	char const *label = nullptr;
	char const *effect = nullptr;
	if (!coinline::audio_route::lookup_command(data, label, effect)) {
		std::string const nd = millennium_audio_trace_telephony_unknown_json(emulated_ns(), cpu_cycle, pc, data);
		millennium_audio_trace_emit_telephony_row(nd);
		return;
	}

	std::string const dec = millennium_audio_trace_telephony_decode_json(emulated_ns(), cpu_cycle, pc, data, label,
		effect);
	millennium_audio_trace_emit_telephony_row(dec);

	m_route->apply_telephony_effect(effect, data, cpu_cycle, pc, label);
}

void millennium_telephony_device::processor_to_host_byte(std::uint8_t data, std::uint64_t cpu_cycle,
	std::uint16_t pc)
{
	std::string const ph = millennium_audio_trace_telephony_processor_to_host_json(emulated_ns(), cpu_cycle, pc,
		data);
	millennium_audio_trace_emit_telephony_row(ph);
	if (m_supervision)
		m_supervision->on_processor_status_byte(data, cpu_cycle, pc);
}

void millennium_telephony_device::notify_modem_dcd(bool dcd_asserted, std::uint64_t cpu_cycle, std::uint16_t pc)
{
	m_route->notify_modem_carrier(dcd_asserted);
	if (m_supervision)
		m_supervision->on_modem_carrier(dcd_asserted, cpu_cycle, pc);
}
