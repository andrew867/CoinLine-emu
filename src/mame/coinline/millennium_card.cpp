// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_card.h"

#include "millennium_firmware.h"

DEFINE_DEVICE_TYPE(MILLENNIUM_CARD, millennium_card_device, "millennium_card", "CoinLine magnetic card reader")

millennium_card_device::millennium_card_device(machine_config const &mconfig, char const *tag, device_t *owner,
	u32 clock)
	: device_t(mconfig, MILLENNIUM_CARD, tag, owner, clock)
{
}

void millennium_card_device::device_start()
{
	std::string err;
	char const *p = osd_getenv("COINLINE_MAG_CARD");
	std::string const rel =
		(p && *p) ? std::string(p) : std::string("fixtures/cards/magcard-valid.json");
	std::vector<std::uint8_t> raw;
	std::string path = rel;
	if (!millennium_read_file(path, raw, err)) {
		char const *root = osd_getenv("COINLINE_EMU_ROOT");
		if (root && *root) {
			std::string r(root);
			while (!r.empty() && (r.back() == '/' || r.back() == '\\'))
				r.pop_back();
			path = r + "/" + rel;
			raw.clear();
			millennium_read_file(path, raw, err);
		}
	}
	if (!raw.empty()) {
		std::string const txt(reinterpret_cast<char const *>(raw.data()), raw.size());
		if (!m_model.parse_fixture_json(txt, err))
			osd_printf_warning("millennium_card: fixture parse failed: %s\n", err.c_str());
	}
}

void millennium_card_device::device_reset()
{
	device_t::device_reset();
	m_model.abort_swipe();
}

bool millennium_card_device::reload_fixture_from_path(std::string const &path, std::string &error_out)
{
	std::vector<std::uint8_t> raw;
	if (!millennium_read_file(path, raw, error_out))
		return false;
	std::string const txt(reinterpret_cast<char const *>(raw.data()), raw.size());
	return m_model.parse_fixture_json(txt, error_out);
}

void millennium_card_device::arm_swipe(u64 cpu_cycle, u64 cpu_hz)
{
	m_model.arm_swipe(cpu_cycle, cpu_hz);
}
