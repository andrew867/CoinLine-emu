// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "emu.h"

DECLARE_DEVICE_TYPE(MILLENNIUM_HOSTBRIDGE, millennium_hostbridge_device)

class millennium_telephony_device;

// Transport endpoint binding lives out-of-process / helper binaries per license isolation.
// The machine exposes this device tag so the driver and scenarios can attach transcripts.
// Call-state is not injected here: bytes flow to `millennium_telephony_device`, which updates route/supervision from hardware maps.
class millennium_hostbridge_device : public device_t {
public:
	millennium_hostbridge_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock = 0);

	void device_start() override ATTR_COLD;
	void device_reset() override;

	/// Forward host→processor bridge bytes into `millennium_telephony_device` when wired.
	void deliver_host_to_processor_byte(std::uint8_t data, std::uint64_t cpu_cycle, std::uint16_t pc);

	/// Telephony processor→host bytes (RX) — not injected as internal `disconnect_event` state.
	void deliver_processor_to_host_byte(std::uint8_t data, std::uint64_t cpu_cycle, std::uint16_t pc);

private:
	optional_device<millennium_telephony_device> m_telephony;
};
