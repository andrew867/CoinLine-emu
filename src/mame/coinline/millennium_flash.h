// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "emu.h"
#include "millennium_flash_model.h"

#include <fstream>
#include <string>

class millennium_flash_device : public device_t, public device_nvram_interface {
public:
	millennium_flash_device(machine_config const &config, char const *tag,
		device_t *owner, std::uint32_t clock = 0);

	void seed(std::uint8_t const *data, std::string const &sha256, bool enabled);
	std::uint8_t read(std::uint32_t address);
	void write(std::uint32_t address, std::uint8_t value);
	std::uint8_t raw(std::uint32_t address) const;

protected:
	void device_start() override;
	void device_stop() override;
	void nvram_default() override;
	bool nvram_read(util::read_stream &file) override;
	bool nvram_write(util::write_stream &file) override;
	bool nvram_can_write() const override { return m_enabled; }

private:
	std::uint64_t now_us() const;
	void trace_event(char const *event, unsigned chip, std::uint32_t address,
		std::uint8_t value, std::uint8_t previous, std::uint8_t erase_mask,
		bool program_error, std::uint64_t emulated_us);
	std::array<millennium_flash_chip, 2> m_chips;
	std::string m_seed_sha256;
	std::ofstream m_trace;
	std::uint64_t m_program_begin_count = 0;
	std::uint64_t m_program_complete_count = 0;
	std::uint64_t m_erase_begin_count = 0;
	std::uint64_t m_erase_complete_count = 0;
	bool m_enabled = false;
};

DECLARE_DEVICE_TYPE(MILLENNIUM_FLASH, millennium_flash_device)
