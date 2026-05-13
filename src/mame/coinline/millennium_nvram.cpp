// SPDX-License-Identifier: GPL-2.0-or-later

#include "emu.h"
#include "ioprocs.h"

#include "millennium_nvram.h"

#include "millennium_firmware.h"
#include "millennium_state.h"

namespace {

std::string resolve_relative_path(device_t const &, std::string const &rel)
{
	if (rel.size() >= 2 && rel[1] == ':')
		return rel;
	if (!rel.empty() && (rel[0] == '/' || rel[0] == '\\'))
		return rel;
	char const *root = osd_getenv("COINLINE_EMU_ROOT");
	if (root && *root) {
		std::string r(root);
		while (!r.empty() && (r.back() == '/' || r.back() == '\\'))
			r.pop_back();
		return r + "/" + rel;
	}
	return rel;
}

} // namespace

DEFINE_DEVICE_TYPE(MILLENNIUM_NVRAM, millennium_nvram_device, "millennium_nvram", "CoinLine NVRAM / table / DLA staging")

millennium_nvram_device::millennium_nvram_device(machine_config const &mconfig, char const *tag,
	device_t *owner, u32 clock)
	: device_t(mconfig, MILLENNIUM_NVRAM, tag, owner, clock)
	, device_nvram_interface(mconfig, *this)
{
}

void millennium_nvram_device::device_start()
{
}

void millennium_nvram_device::device_reset()
{
	device_t::device_reset();
	m_model.reset_session();
}

void millennium_nvram_device::nvram_default()
{
	millennium_state *const st = dynamic_cast<millennium_state *>(owner());
	if (!st) {
		osd_printf_warning("millennium_nvram: invalid owner (expected millennium_state)\n");
		return;
	}
	m_model.configure(st->memory_layout());

	char const *nvpath = osd_getenv("COINLINE_NVRAM");
	std::string const rel =
		(nvpath && *nvpath) ? std::string(nvpath) : std::string("fixtures/nvram/factory-default.nvram.json");
	std::string const path = resolve_relative_path(*this, rel);
	std::vector<std::uint8_t> raw;
	std::string err;
	if (!millennium_read_file(path, raw, err)) {
		osd_printf_warning("millennium_nvram: cannot read initial image %s (%s); zero-filled\n", path.c_str(),
			err.c_str());
		std::string ign;
		m_model.recover_default_cleared(ign);
		return;
	}
	std::string const txt(reinterpret_cast<char const *>(raw.data()), raw.size());
	if (!m_model.load_envelope_json(txt, err))
		osd_printf_warning("millennium_nvram: parsing initial image %s failed (%s)\n", path.c_str(), err.c_str());
	if (m_model.checksum_failure())
		osd_printf_info("millennium_nvram: checksum mismatch on load — firmware recovery expected\n");
	// Blank / never-programmed image: validity sentinels at fixed offsets are zero; RTOS init is expected to program them.
	if (!m_model.checksum_failure() && m_model.read_nvram(0) == 0U && m_model.read_nvram(1) == 0U && m_model.read_nvram(30) == 0U
		&& m_model.read_nvram(31) == 0U)
		// ASCII hyphen: Windows consoles often mis-render U+2014 em dash from osd_printf_info.
		osd_printf_info(
			"millennium_nvram: initial image has blank validity fields - first-boot EEPROM/NVRAM programming expected\n");
}

bool millennium_nvram_device::nvram_read(util::read_stream &file)
{
	auto *const rr = dynamic_cast<util::random_read *>(&file);
	std::uint64_t len_u64 = 0;
	if (!rr || rr->length(len_u64))
		return false;
	std::vector<u8> nvbuf((std::size_t(len_u64)));
	auto const [err, actual] = util::read(file, nvbuf.data(), nvbuf.size());
	if (err || actual != nvbuf.size())
		return false;
	std::string e;
	millennium_state *const st = dynamic_cast<millennium_state *>(owner());
	if (st)
		m_model.configure(st->memory_layout());
	if (!m_model.deserialize_state(nvbuf, e))
		return false;

	// Guard against persisted stale images that keep firmware in perpetual
	// not-installed/OOS branches. Recover to firmware-valid defaults when
	// essential EEPROM sentinel fields are missing.
	u16 const first_valid = u16(m_model.read_nvram(0)) | (u16(m_model.read_nvram(1)) << 8);
	u16 const term_inst = u16(m_model.read_nvram(30)) | (u16(m_model.read_nvram(31)) << 8);
	u16 const last_valid = u16(m_model.read_nvram(40)) | (u16(m_model.read_nvram(41)) << 8);
	if (first_valid != 0xa5c4U || term_inst != 0x1f2eU || last_valid != 0x5a3bU) {
		std::string ign;
		m_model.recover_default_cleared(ign);
	}
	return true;
}

bool millennium_nvram_device::nvram_write(util::write_stream &file)
{
	std::vector<u8> blob;
	m_model.serialize_state(blob);
	auto const [err, actual] = util::write(file, blob.data(), blob.size());
	return !err && actual == blob.size();
}
