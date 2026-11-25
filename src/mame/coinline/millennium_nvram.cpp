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

	std::string err;
	for (u32 i = 0; i < millennium_microwire_93c66::k_capacity_bytes; ++i)
		m_model.write_nvram(i, 0xffU, err);

	char const *const nvpath = osd_getenv("COINLINE_NVRAM");
	if (!nvpath || !*nvpath)
		return;
	std::string const path = resolve_relative_path(*this, nvpath);
	std::vector<std::uint8_t> raw;
	if (!millennium_read_file(path, raw, err)) {
		osd_printf_warning("millennium_nvram: cannot read initial image %s (%s)\n", path.c_str(), err.c_str());
		return;
	}
	auto initial = m_model;
	std::string const txt(raw.begin(), raw.end());
	if (initial.load_envelope_json(txt, err))
		m_model = std::move(initial);
	else
		osd_printf_warning("millennium_nvram: parsing initial image %s failed (%s)\n", path.c_str(), err.c_str());
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
	return m_model.deserialize_state(nvbuf, e);
}

bool millennium_nvram_device::nvram_write(util::write_stream &file)
{
	std::vector<u8> blob;
	m_model.serialize_state(blob);
	auto const [err, actual] = util::write(file, blob.data(), blob.size());
	return !err && actual == blob.size();
}
