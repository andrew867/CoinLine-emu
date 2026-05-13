// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_smartcard.h"

#include "millennium_firmware.h"

DEFINE_DEVICE_TYPE(MILLENNIUM_SMARTCARD, millennium_smartcard_device, "millennium_smartcard",
	"CoinLine ISO7816 smart card")

millennium_smartcard_device::millennium_smartcard_device(machine_config const &mconfig, char const *tag,
	device_t *owner, u32 clock)
	: device_t(mconfig, MILLENNIUM_SMARTCARD, tag, owner, clock)
{
}

void millennium_smartcard_device::device_start()
{
	std::string err;
	char const *p = osd_getenv("COINLINE_SMART_CARD");
	std::string const rel =
		(p && *p) ? std::string(p) : std::string("fixtures/cards/smartcard-valid.json");
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
			osd_printf_warning("millennium_smartcard: fixture parse failed: %s\n", err.c_str());
	}
}

void millennium_smartcard_device::device_reset()
{
	device_t::device_reset();
	m_model.remove_card();
}

bool millennium_smartcard_device::reload_fixture_from_path(std::string const &path, std::string &error_out)
{
	std::vector<std::uint8_t> raw;
	if (!millennium_read_file(path, raw, error_out))
		return false;
	std::string const txt(reinterpret_cast<char const *>(raw.data()), raw.size());
	return m_model.parse_fixture_json(txt, error_out);
}

void millennium_smartcard_device::insert_card(u64 cycle, u64 cpu_hz)
{
	m_model.insert_card_at(cycle, cpu_hz);
}

void millennium_smartcard_device::remove_card()
{
	m_model.remove_card();
}

std::uint8_t millennium_smartcard_device::read_fifo(u64 cycle)
{
	return m_model.read_fifo(cycle);
}

void millennium_smartcard_device::write_command(u8 data, u64 cycle)
{
	m_model.write_command(data, cycle);
}

std::uint8_t millennium_smartcard_device::status_lines() const
{
	return m_model.status_lines();
}

void millennium_smartcard_device::notify_reset(u64 cycle)
{
	m_model.notify_reset(cycle);
}
