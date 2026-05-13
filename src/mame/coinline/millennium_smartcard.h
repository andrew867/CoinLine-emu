// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "emu.h"

#include "millennium_smartcard_model.h"

DECLARE_DEVICE_TYPE(MILLENNIUM_SMARTCARD, millennium_smartcard_device)

class millennium_smartcard_device : public device_t {
public:
	millennium_smartcard_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock = 0);

	void device_start() override ATTR_COLD;
	void device_reset() override;

	bool reload_fixture_from_path(std::string const &path, std::string &error_out);
	void insert_card(u64 cycle, u64 cpu_hz);
	void remove_card();
	std::uint8_t read_fifo(u64 cycle);
	void write_command(u8 data, u64 cycle);
	std::uint8_t status_lines() const;

	void notify_reset(u64 cycle);

	millennium_smartcard_model &model() noexcept { return m_model; }

private:
	millennium_smartcard_model m_model{};
};
