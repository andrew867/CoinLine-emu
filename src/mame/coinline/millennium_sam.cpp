// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_sam.h"
#include "millennium_firmware.h"

DEFINE_DEVICE_TYPE(MILLENNIUM_SAM, millennium_sam_device, "millennium_sam",
	"CoinLine Millennium internal contact module board")

millennium_sam_device::millennium_sam_device(machine_config const &mconfig, char const *tag, device_t *owner,
	u32 clock)
	: device_t(mconfig, MILLENNIUM_SAM, tag, owner, clock)
{
}

void millennium_sam_device::device_start()
{
	char const *trace = osd_getenv("COINLINE_SAM_PIN_TRACE");
	m_pin_trace = trace && *trace == '1';
	for (unsigned slot = 0; slot < 3U; ++slot) {
		std::string const name = "COINLINE_SAM_SLOT" + std::to_string(slot);
		char const *path = osd_getenv(name.c_str());
		if (!path || !*path)
			continue;
		std::string error;
		std::vector<u8> raw;
		if (!millennium_read_file(path, raw, error) ||
			!m_contacts.configure_lab_slot(slot, std::string(raw.begin(), raw.end()), 0, 12288000, error))
			osd_printf_warning("internal contact slot %u fixture rejected: %s\n", slot, error.c_str());
	}
}

void millennium_sam_device::device_reset()
{
	device_t::device_reset();
	m_contacts.select(std::nullopt, 0);
	for (unsigned slot = 0; slot < 3U; ++slot)
		m_contacts.pins(slot, false, false, false, 0);
	m_last_pins.reset();
}

void millennium_sam_device::observe_later_pins(u8 h, u8 d, u8 e, u8 c, u8 g, u64 cycle)
{
	auto const selection = millennium_async_contact_selector(h, d);
	auto const internal = selection && *selection < 3U ? selection : std::nullopt;
	bool const was_internal = m_contacts.selected().has_value();
	m_contacts.later_pins(h, d, e, c, g, cycle);
	if (!m_pin_trace || m_trace_count >= 4096 || (!was_internal && !internal))
		return;
	// Deduplicate on the contact selector, supply, clock and reset lines.
	// Other bits on these shared latches (notably periodic supervision)
	// must not consume the bounded contact observation budget.
	u64 const packed = (u64(h & 0x08U) << 32) | (u64(d & 0x03U) << 24)
		| (u64(e & 0x02U) << 16) | (u64(c & 0x0fU) << 8) | u64(g & 0x3eU)
		| (u64(m_contacts.active()) << 40);
	if (m_last_pins == packed)
		return;
	m_last_pins = packed;
	++m_trace_count;
	osd_printf_info("internal contact pins %llu selection=%d H=%02X D=%02X E=%02X C=%02X G=%02X active=%u\n",
		static_cast<unsigned long long>(cycle), internal ? int(*internal) : -1,
		unsigned(h), unsigned(d), unsigned(e), unsigned(c), unsigned(g), unsigned(m_contacts.active()));
}
