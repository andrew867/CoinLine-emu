// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "emu.h"
#include "millennium_smartcard_model.h"

DECLARE_DEVICE_TYPE(MILLENNIUM_SAM, millennium_sam_device)

// Three internal contact positions, distinct from the customer card and ADSI.
// Unpopulated slots do not respond. The shared serial adapter is owned by
// the contact reader device, while module state and board latches live here.
class millennium_sam_device : public device_t {
public:
	millennium_sam_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock = 0);

	void device_start() override ATTR_COLD;
	void device_reset() override;

	void observe_later_pins(u8 h, u8 d, u8 e, u8 c, u8 g, u64 cycle);
	millennium_smartcard_model *active_card() noexcept { return m_contacts.active_card(); }

private:
	millennium_sam_model m_contacts;
	bool m_pin_trace = false;
	unsigned m_trace_count = 0;
	std::optional<u64> m_last_pins;
};
