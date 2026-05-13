// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "emu.h"

#include "millennium_nvram_model.h"

DECLARE_DEVICE_TYPE(MILLENNIUM_NVRAM, millennium_nvram_device)

class millennium_nvram_device : public device_t, public device_nvram_interface {
public:
	millennium_nvram_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock = 0);

	void device_start() override ATTR_COLD;
	void device_reset() override;

	void nvram_default() override;
	bool nvram_read(util::read_stream &file) override;
	bool nvram_write(util::write_stream &file) override;

	millennium_nvram_model &model() noexcept { return m_model; }

private:
	millennium_nvram_model m_model{};
};
