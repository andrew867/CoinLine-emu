// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "emu.h"

#include "cpu/z180/z180.h"

#include "millennium_supervision_fsm.h"

DECLARE_DEVICE_TYPE(MILLENNIUM_SUPERVISION, millennium_supervision_device)

/// Disconnect supervision — consumes processor→host telephony status codes + modem carrier edges.
class millennium_supervision_device : public device_t {
public:
	millennium_supervision_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock = 0);

	void device_start() override ATTR_COLD;
	void device_reset() override;

	/// Telephony bridge RX — status bytes from telephony processor toward host (feeds FSM when mapped).
	void on_processor_status_byte(std::uint8_t code, std::uint64_t cpu_cycle, std::uint16_t pc);

	/// Modem DCD / carrier — fused with status codes for disconnect classification.
	void on_modem_carrier(bool carrier_up, std::uint64_t cpu_cycle, std::uint16_t pc);

	/// Watchdog expiry (`timers.cutoff_on_disconnect_duration` — profile ticks; optional explicit fire for tests).
	void on_watchdog_fired(std::uint64_t cpu_cycle, std::uint16_t pc);

private:
	std::uint64_t emulated_ns() const;
	void emit_fsm_events(std::vector<coinline::supervision::supervision_fsm_event> const &events);

	required_device<z180_device> m_maincpu;
	coinline::supervision::disconnect_supervision_fsm m_fsm{};
};
