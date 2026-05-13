// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "emu.h"

#include <cstdint>

DECLARE_DEVICE_TYPE(MILLENNIUM_TELEPHONY, millennium_telephony_device)

class millennium_audio_route_device;
class millennium_supervision_device;

/// Decodes host→telephony-processor bytes per `audio-routing-state-map.json` and drives `millennium_audio_route_device`;
/// processor→host bytes feed `millennium_supervision_device` (no scenario-injected disconnect state).
class millennium_telephony_device : public device_t {
public:
	millennium_telephony_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock = 0);

	void device_start() override ATTR_COLD;
	void device_reset() override;

	/// Host→processor channel (UART/SIO bridge — logical attachment per board map).
	void host_to_processor_byte(std::uint8_t data, std::uint64_t cpu_cycle, std::uint16_t pc);

	/// Processor→host status bytes (telephony processor toward host) — feeds supervision per `disconnect-supervision-map.json`.
	void processor_to_host_byte(std::uint8_t data, std::uint64_t cpu_cycle, std::uint16_t pc);

	/// Modem DCD (carrier) — `dcd_asserted` true means carrier up; passes CPU context for traces.
	void notify_modem_dcd(bool dcd_asserted, std::uint64_t cpu_cycle, std::uint16_t pc);

private:
	required_device<millennium_audio_route_device> m_route;
	optional_device<millennium_supervision_device> m_supervision;

	std::uint64_t emulated_ns() const;
};
