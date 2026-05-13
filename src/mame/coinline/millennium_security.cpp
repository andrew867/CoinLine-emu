// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_security.h"

DEFINE_DEVICE_TYPE(MILLENNIUM_SECURITY, millennium_security_device, "millennium_security",
	"CoinLine Millennium security inputs")

millennium_security_device::millennium_security_device(machine_config const &mconfig, char const *tag,
	device_t *owner, u32 clock)
	: device_t(mconfig, MILLENNIUM_SECURITY, tag, owner, clock)
	, m_sec(*owner, "SECMASK")
{
}

void millennium_security_device::device_start()
{
	// Lock / door / vault / service lines are sampled through SECMASK with debounce from the board profile.
}

void millennium_security_device::device_reset()
{
	device_t::device_reset();
	m_model.reset();
}

void millennium_security_device::apply_config(millennium_security_board_config const &cfg)
{
	m_model.configure(cfg);
	m_model.reset();
}

u8 millennium_security_device::read(std::uint64_t cpu_cycle)
{
	u32 const bits = m_sec->read();
	return m_model.read_lines(bits, cpu_cycle);
}
