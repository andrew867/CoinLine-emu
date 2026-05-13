// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "emu.h"

#include "millennium_vfd_model.h"

#include <vector>

DECLARE_DEVICE_TYPE(MILLENNIUM_VFD, millennium_vfd_device)

class millennium_vfd_device : public device_t {
public:
	millennium_vfd_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock = 0);

	void device_start() override ATTR_COLD;
	void device_reset() override;

	void apply_display_profile(millennium_display_profile const &profile);
	u8 read_status(std::uint64_t cpu_cycle);
	void write_port(std::uint8_t data, std::uint64_t cpu_cycle);
	/// \p pio_port_b is 82C55 port B shadow (`VFD_CSB` bit 6, `VFDA0` bit 5).
	void write_port(std::uint8_t data, std::uint64_t cpu_cycle, std::uint8_t pio_port_b);

	bool milestone_m6_met() const noexcept { return m_model.milestone_m6_met(); }
	millennium_vfd_model const &buffer() const noexcept { return m_model; }

	std::string export_snapshot_json() const { return m_model.export_snapshot_json(); }
	std::string first_text_row() const { return m_model.first_text_row(); }
	bool unknown_escape_pending() const noexcept { return m_model.unknown_escape_pending(); }

	std::vector<char> const &vfd_cells() const noexcept { return m_model.cells(); }
	millennium_display_profile const &display_profile() const noexcept { return m_model.display_profile(); }
	std::size_t firmware_port_byte_writes() const noexcept { return m_model.firmware_write_bytes(); }

private:
	millennium_vfd_model m_model{};
};
