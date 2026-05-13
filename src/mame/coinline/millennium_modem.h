// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "emu.h"

#include "millennium_modem_model.h"

DECLARE_DEVICE_TYPE(MILLENNIUM_MODEM, millennium_modem_device)

/// UART/modem line model. DCD is sampled in `millennium_state::screen_update` and forwarded to telephony / audio route.
class millennium_modem_device : public device_t {
public:
	millennium_modem_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock = 0);

	void device_start() override ATTR_COLD;
	void device_reset() override;

	void note_asci_programmed(u8 cntla, u8 cntlb) { m_model.note_asci_programmed(cntla, cntlb); }
	bool consume_m8_pending() noexcept { return m_model.consume_m8_pending(); }

	bool dcd_line() const noexcept { return m_model.dcd(); }
	bool cts_line() const noexcept { return m_model.cts(); }

	millennium_modem_model &model() noexcept { return m_model; }

private:
	millennium_modem_model m_model{};
};
