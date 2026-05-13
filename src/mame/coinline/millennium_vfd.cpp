// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_vfd.h"

DEFINE_DEVICE_TYPE(MILLENNIUM_VFD, millennium_vfd_device, "millennium_vfd", "CoinLine Millennium VFD")

millennium_vfd_device::millennium_vfd_device(machine_config const &mconfig, char const *tag, device_t *owner,
	u32 clock)
	: device_t(mconfig, MILLENNIUM_VFD, tag, owner, clock)
{
}

void millennium_vfd_device::device_start()
{
}

void millennium_vfd_device::device_reset()
{
	device_t::device_reset();
	m_model.reset();
}

void millennium_vfd_device::apply_display_profile(millennium_display_profile const &profile)
{
	m_model.configure(profile);
	m_model.reset();
}

u8 millennium_vfd_device::read_status(std::uint64_t cpu_cycle)
{
	return m_model.status_read(cpu_cycle);
}

void millennium_vfd_device::write_port(std::uint8_t data, std::uint64_t cpu_cycle)
{
	write_port(data, cpu_cycle, 0U);
}

void millennium_vfd_device::write_port(std::uint8_t data, std::uint64_t cpu_cycle, std::uint8_t pio_port_b)
{
	m_model.write(data, cpu_cycle, pio_port_b);
	char const *const warn = osd_getenv("COINLINE_VFD_WARN_UNKNOWN_ESCAPE");
	if (warn && warn[0] == '1' && warn[1] == '\0' && m_model.unknown_escape_pending())
		osd_printf_warning("millennium_vfd: unknown escape subcommand\n");
}
