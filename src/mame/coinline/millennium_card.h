// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "emu.h"

#include "millennium_card_model.h"

DECLARE_DEVICE_TYPE(MILLENNIUM_CARD, millennium_card_device)

class millennium_card_device : public device_t {
public:
	millennium_card_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock = 0);

	void device_start() override ATTR_COLD;
	void device_reset() override;

	bool reload_fixture_from_path(std::string const &path, std::string &error_out);
	void arm_swipe(u64 cpu_cycle, u64 cpu_hz);

	millennium_card_model &model() noexcept { return m_model; }

private:
	millennium_card_model m_model{};
};
