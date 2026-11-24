// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "emu.h"

DECLARE_DEVICE_TYPE(MILLENNIUM_SAM, millennium_sam_device)

/// Placeholder for the e-purse security-module / SAM daughterboard path (socket detect, ISO7816-style async,
/// alarms such as not responding / locked / expiry). **Proton** vs **Smart City** builds differ in **application
/// messaging** (card-handler layers); they share the same board async/SAM plumbing already modeled via MACH PIO
/// and the ISO7816 smart-card device. No extra silicon — protocol fixtures still TODO.
class millennium_sam_device : public device_t {
public:
	millennium_sam_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock = 0);

	void device_start() override ATTR_COLD;
	void device_reset() override;

	/// Until fixtures exist, the module is treated as absent / non-responsive (terminal alarms would apply on real hardware).
	bool module_responding() const noexcept { return false; }

private:
	bool m_stub_logged = false;
};
