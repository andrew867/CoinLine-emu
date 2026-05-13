// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "emu.h"

#include "millennium_security_model.h"

DECLARE_DEVICE_TYPE(MILLENNIUM_SECURITY, millennium_security_device)

class millennium_security_device : public device_t {
public:
	millennium_security_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock = 0);

	void device_start() override ATTR_COLD;
	void device_reset() override;

	void apply_config(millennium_security_board_config const &cfg);

	u8 read(std::uint64_t cpu_cycle);

private:
	required_ioport m_sec;
	millennium_security_model m_model{};
};
