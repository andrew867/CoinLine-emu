// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_hostbridge.h"

#include "millennium_telephony.h"

DEFINE_DEVICE_TYPE(MILLENNIUM_HOSTBRIDGE, millennium_hostbridge_device, "millennium_hostbridge",
	"CoinLine Host Bridge (byte pipe)")

millennium_hostbridge_device::millennium_hostbridge_device(machine_config const &mconfig, char const *tag,
	device_t *owner, u32 clock)
	: device_t(mconfig, MILLENNIUM_HOSTBRIDGE, tag, owner, clock)
	, m_telephony(*this, "^:telephony")
{
}

void millennium_hostbridge_device::device_start()
{
}

void millennium_hostbridge_device::device_reset()
{
}

void millennium_hostbridge_device::deliver_host_to_processor_byte(std::uint8_t data, std::uint64_t cpu_cycle,
	std::uint16_t pc)
{
	if (m_telephony)
		m_telephony->host_to_processor_byte(data, cpu_cycle, pc);
}

void millennium_hostbridge_device::deliver_processor_to_host_byte(std::uint8_t data, std::uint64_t cpu_cycle,
	std::uint16_t pc)
{
	if (m_telephony)
		m_telephony->processor_to_host_byte(data, cpu_cycle, pc);
}
