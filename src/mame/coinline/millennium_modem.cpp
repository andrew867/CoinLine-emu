// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_modem.h"

DEFINE_DEVICE_TYPE(MILLENNIUM_MODEM, millennium_modem_device, "millennium_modem", "CoinLine Modem UART")

millennium_modem_device::millennium_modem_device(machine_config const &mconfig, char const *tag,
	device_t *owner, u32 clock)
	: device_t(mconfig, MILLENNIUM_MODEM, tag, owner, clock)
{
}

void millennium_modem_device::device_start()
{
}

void millennium_modem_device::device_reset()
{
	m_model.reset();
}
