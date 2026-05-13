// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_sam.h"

DEFINE_DEVICE_TYPE(MILLENNIUM_SAM, millennium_sam_device, "millennium_sam",
	"CoinLine Millennium SAM / e-purse module (stub)")

millennium_sam_device::millennium_sam_device(machine_config const &mconfig, char const *tag, device_t *owner,
	u32 clock)
	: device_t(mconfig, MILLENNIUM_SAM, tag, owner, clock)
{
}

void millennium_sam_device::device_start()
{
	if (!m_stub_logged) {
		m_stub_logged = true;
		osd_printf_verbose("millennium_sam: stub device — no e-purse/SAM protocol or module image loaded.\n");
	}
}

void millennium_sam_device::device_reset()
{
	device_t::device_reset();
}
