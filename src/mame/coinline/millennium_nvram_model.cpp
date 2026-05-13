// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_nvram_model.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>

namespace {

bool parse_json_string(std::string const &json, char const *key, std::string &out)
{
	auto const kpos = json.find(key);
	if (kpos == std::string::npos)
		return false;
	auto const colon = json.find(':', kpos);
	if (colon == std::string::npos)
		return false;
	auto q1 = json.find('"', colon);
	if (q1 == std::string::npos)
		return false;
	auto q2 = json.find('"', q1 + 1);
	if (q2 == std::string::npos || q2 <= q1)
		return false;
	out = json.substr(q1 + 1, q2 - q1 - 1);
	return true;
}

bool parse_json_uint(std::string const &json, char const *key, std::uint32_t &out)
{
	auto const kpos = json.find(key);
	if (kpos == std::string::npos)
		return false;
	auto const colon = json.find(':', kpos);
	if (colon == std::string::npos)
		return false;
	std::size_t i = colon + 1;
	while (i < json.size() && std::isspace(static_cast<unsigned char>(json[i])))
		++i;
	std::uint32_t v = 0;
	if (i >= json.size())
		return false;
	while (i < json.size() && std::isdigit(static_cast<unsigned char>(json[i]))) {
		v = v * 10U + std::uint32_t(json[i] - '0');
		++i;
	}
	out = v;
	return true;
}

int base64_value(char c)
{
	if (c >= 'A' && c <= 'Z')
		return int(c - 'A');
	if (c >= 'a' && c <= 'z')
		return int(c - 'a') + 26;
	if (c >= '0' && c <= '9')
		return int(c - '0') + 52;
	if (c == '+')
		return 62;
	if (c == '/')
		return 63;
	return -1;
}

bool base64_decode(std::string const &in, std::vector<std::uint8_t> &out, std::string &error_out)
{
	out.clear();
	std::vector<unsigned char> buf;
	buf.reserve(in.size());
	for (char ch : in) {
		if (std::isspace(static_cast<unsigned char>(ch)))
			continue;
		if (ch == '=')
			break;
		int v = base64_value(ch);
		if (v < 0) {
			error_out = "invalid base64 character";
			return false;
		}
		buf.push_back(static_cast<unsigned char>(v));
	}
	out.reserve(buf.size() * 3 / 4 + 4);
	for (std::size_t i = 0; i + 3 < buf.size(); i += 4) {
		std::uint32_t n = std::uint32_t(buf[i]) << 18;
		n |= std::uint32_t(buf[i + 1]) << 12;
		n |= std::uint32_t(buf[i + 2]) << 6;
		n |= std::uint32_t(buf[i + 3]);
		out.push_back(static_cast<std::uint8_t>((n >> 16) & 0xff));
		out.push_back(static_cast<std::uint8_t>((n >> 8) & 0xff));
		out.push_back(static_cast<std::uint8_t>(n & 0xff));
	}
	return true;
}

void append_hex_byte(std::ostringstream &oss, std::uint8_t b)
{
	static char const digits[] = "0123456789ABCDEF";
	oss << digits[(b >> 4) & 0xf];
	oss << digits[b & 0xf];
}

std::string base64_encode(std::vector<std::uint8_t> const &data)
{
	static char const tbl[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	std::string out;
	out.reserve(((data.size() + 2) / 3) * 4);
	for (std::size_t i = 0; i < data.size(); i += 3) {
		std::uint32_t n = std::uint32_t(data[i]) << 16;
		int pad = 0;
		if (i + 1 < data.size())
			n |= std::uint32_t(data[i + 1]) << 8;
		else
			pad = 2;
		if (i + 2 < data.size())
			n |= std::uint32_t(data[i + 2]);
		else if (pad == 0)
			pad = 1;
		out.push_back(tbl[(n >> 18) & 63]);
		out.push_back(tbl[(n >> 12) & 63]);
		out.push_back(pad >= 2 ? '=' : tbl[(n >> 6) & 63]);
		out.push_back(pad >= 1 ? '=' : tbl[n & 63]);
	}
	return out;
}

} // namespace

void millennium_nvram_model::configure(millennium_memory_layout_config const &layout)
{
	m_layout = layout;
	pad_vectors();
}

void millennium_nvram_model::pad_vectors()
{
	m_nvram.assign(size_t(m_layout.nvram_size ? m_layout.nvram_size : 0), 0);
	m_table.assign(size_t(m_layout.table_storage_size ? m_layout.table_storage_size : 0), 0);
	m_dla.assign(size_t(m_layout.dla_stage_size ? m_layout.dla_stage_size : 0), 0);
}

void millennium_nvram_model::reset_session()
{
	// Battery-backed contents persist across CPU/machine reset; session hook reserved for future use (e.g. clear DLA).
}

std::uint8_t millennium_nvram_model::compute_sum8(std::vector<std::uint8_t> const &data)
{
	std::uint32_t s = 0;
	for (auto b : data)
		s += b;
	return static_cast<std::uint8_t>(s & 0xffU);
}

bool millennium_nvram_model::parse_hex_u8(std::string const &hex, std::uint8_t &out)
{
	std::size_t i = 0;
	if (hex.size() >= 2 && hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X'))
		i = 2;
	if (i >= hex.size())
		return false;
	std::uint32_t v = 0;
	for (; i < hex.size(); ++i) {
		char c = hex[i];
		int d = -1;
		if (c >= '0' && c <= '9')
			d = c - '0';
		else if (c >= 'A' && c <= 'F')
			d = c - 'A' + 10;
		else if (c >= 'a' && c <= 'f')
			d = c - 'a' + 10;
		else
			return false;
		v = (v << 4) | std::uint32_t(d);
		if (v > 0xffU)
			return false;
	}
	out = static_cast<std::uint8_t>(v);
	return true;
}

bool millennium_nvram_model::load_envelope_json(std::string const &json_text, std::string &error_out)
{
	m_checksum_failure = false;
	m_use_sum8_envelope = false;
	std::uint32_t size = 0;
	if (!parse_json_uint(json_text, "\"size\"", size) || size == 0) {
		error_out = "missing or invalid size";
		return false;
	}
	if (m_layout.nvram_size != 0 && size != m_layout.nvram_size) {
		error_out = "NVRAM JSON size does not match board profile memory.nvram_size";
		return false;
	}
	std::string algo = "sum8";
	(void)parse_json_string(json_text, "\"checksum_algorithm\"", algo);

	std::string checksum_field;
	if (!parse_json_string(json_text, "\"checksum_value\"", checksum_field)) {
		error_out = "missing checksum_value";
		return false;
	}

	std::string data_b64;
	(void)parse_json_string(json_text, "\"data_b64\"", data_b64);

	m_nvram.assign(size_t(size), 0);
	if (data_b64.empty()) {
		// Empty fixture payload means "factory defaults", not a literal all-zero EEPROM.
		recover_default_cleared(error_out);
		return true;
	}
	if (!data_b64.empty()) {
		std::string b64_err;
		if (!base64_decode(data_b64, m_nvram, b64_err)) {
			error_out = b64_err;
			return false;
		}
	}
	if (m_nvram.size() < size_t(size))
		m_nvram.resize(size_t(size), 0);
	else if (m_nvram.size() > size_t(size))
		m_nvram.resize(size_t(size));

	if (algo == "none") {
		return true;
	}
	if (algo == "sum8") {
		std::uint8_t expected = 0;
		if (!parse_hex_u8(checksum_field, expected)) {
			error_out = "bad checksum_value hex";
			return false;
		}
		std::uint8_t const got = compute_sum8(m_nvram);
		if (got != expected) {
			m_checksum_failure = true;
			std::ostringstream oss;
			oss << "checksum mismatch: expected 0x";
			append_hex_byte(oss, expected);
			oss << " got 0x";
			append_hex_byte(oss, got);
			error_out = oss.str();
			return true; // loaded bytes; firmware may recover
		}
		return true;
	}
	error_out = "unsupported checksum_algorithm";
	return false;
}

bool millennium_nvram_model::save_envelope_json(std::string &json_out, std::string &error_out) const
{
	(void)error_out;
	std::uint8_t const sum = compute_sum8(m_nvram);
	std::ostringstream oss;
	oss << "{\n";
	oss << "  \"version\": \"1.0\",\n";
	oss << "  \"size\": " << m_nvram.size() << ",\n";
	oss << "  \"checksum_algorithm\": \"sum8\",\n";
	oss << "  \"checksum_value\": \"0x";
	append_hex_byte(oss, sum);
	oss << "\",\n";
	oss << "  \"data_b64\": \"" << base64_encode(m_nvram) << "\"\n";
	oss << "}\n";
	json_out = oss.str();
	return true;
}

std::uint8_t millennium_nvram_model::read_nvram(std::uint32_t offset) const
{
	if (offset >= m_nvram.size())
		return 0xff;
	return m_nvram[size_t(offset)];
}

bool millennium_nvram_model::write_nvram(std::uint32_t offset, std::uint8_t value, std::string &error_out)
{
	if (offset >= m_nvram.size()) {
		error_out = "nvram write out of range";
		m_log.push_back("reject nvram off=" + std::to_string(offset));
		return false;
	}
	m_nvram[size_t(offset)] = value;
	m_use_sum8_envelope = true;
	m_envelope_sum8 = compute_sum8(m_nvram);
	return true;
}

std::uint8_t millennium_nvram_model::read_table(std::uint32_t offset) const
{
	if (offset >= m_table.size())
		return 0xff;
	return m_table[size_t(offset)];
}

bool millennium_nvram_model::write_table(std::uint32_t offset, std::uint8_t value, std::string &error_out)
{
	if (offset >= m_table.size()) {
		error_out = "table_storage write out of range";
		m_log.push_back("reject table off=" + std::to_string(offset));
		return false;
	}
	m_table[size_t(offset)] = value;
	return true;
}

std::uint8_t millennium_nvram_model::read_dla(std::uint32_t offset) const
{
	if (offset >= m_dla.size())
		return 0xff;
	return m_dla[size_t(offset)];
}

bool millennium_nvram_model::write_dla(std::uint32_t offset, std::uint8_t value, std::string &error_out)
{
	if (offset >= m_dla.size()) {
		error_out = "dla staging write out of range";
		m_log.push_back("reject dla off=" + std::to_string(offset));
		return false;
	}
	m_dla[size_t(offset)] = value;
	return true;
}

bool millennium_nvram_model::verify_checksum(std::string &error_out) const
{
	if (m_nvram.empty()) {
		error_out = "empty nvram buffer";
		return false;
	}
	if (m_checksum_failure)
		return false;
	if (!m_use_sum8_envelope)
		return true;
	return compute_sum8(m_nvram) == m_envelope_sum8;
}

void millennium_nvram_model::recover_default_cleared(std::string &error_out)
{
	(void)error_out;
	std::fill(m_nvram.begin(), m_nvram.end(), 0);
	// Seed minimal EEPROM-valid defaults used by INITASK:
	// - first_validity_flag  = 0xA5C4
	// - terminal_installed   = 0x1F2E (EEPROM_ALL_INSTALLED)
	// - last_validity_flag   = ~0xA5C4 = 0x5A3B
	// Offsets match the terminal EEPROM layout consumed by the boot/init task.
	if (m_nvram.size() >= 42U) {
		m_nvram[0] = 0xc4U;
		m_nvram[1] = 0xa5U;
		m_nvram[30] = 0x2eU;
		m_nvram[31] = 0x1fU;
		m_nvram[40] = 0x3bU;
		m_nvram[41] = 0x5aU;
	}
	m_checksum_failure = false;
	m_use_sum8_envelope = true;
	m_envelope_sum8 = compute_sum8(m_nvram);
}

bool millennium_nvram_model::apply_dla_to_flash(std::vector<std::uint8_t> &flash_rom, std::string &error_out)
{
	if (flash_rom.empty()) {
		error_out = "empty flash buffer";
		return false;
	}
	std::size_t const n = std::min(m_dla.size(), flash_rom.size());
	if (n == 0) {
		error_out = "nothing to apply";
		return false;
	}
	std::memcpy(flash_rom.data(), m_dla.data(), n);
	return true;
}

namespace {
constexpr std::uint8_t k_magic_bytes[6] = {'C', 'L', 'N', 'V', '0', '1'};
} // namespace

bool millennium_nvram_model::serialize_state(std::vector<std::uint8_t> &out) const
{
	std::uint32_t const nv = std::uint32_t(m_nvram.size());
	std::uint32_t const tb = std::uint32_t(m_table.size());
	std::uint32_t const dl = std::uint32_t(m_dla.size());
	std::vector<std::uint8_t> payload;
	payload.reserve(6 + 16 + m_nvram.size() + m_table.size() + m_dla.size());
	for (std::uint8_t b : k_magic_bytes)
		payload.push_back(b);
	auto push_u32 = [&](std::uint32_t v) {
		payload.push_back(std::uint8_t(v & 0xff));
		payload.push_back(std::uint8_t((v >> 8) & 0xff));
		payload.push_back(std::uint8_t((v >> 16) & 0xff));
		payload.push_back(std::uint8_t((v >> 24) & 0xff));
	};
	push_u32(1);
	push_u32(nv);
	push_u32(tb);
	push_u32(dl);
	payload.insert(payload.end(), m_nvram.begin(), m_nvram.end());
	payload.insert(payload.end(), m_table.begin(), m_table.end());
	payload.insert(payload.end(), m_dla.begin(), m_dla.end());
	out = std::move(payload);
	return true;
}

bool millennium_nvram_model::deserialize_state(std::vector<std::uint8_t> const &in, std::string &error_out)
{
	if (in.size() < 6 + 16) {
		error_out = "nvram blob too small";
		return false;
	}
	if (std::memcmp(in.data(), k_magic_bytes, 6) != 0) {
		error_out = "nvram blob bad magic";
		return false;
	}
	std::size_t o = 6;
	auto read_u32 = [&](std::uint32_t &v) -> bool {
		if (o + 4 > in.size())
			return false;
		v = std::uint32_t(in[o]) | (std::uint32_t(in[o + 1]) << 8) | (std::uint32_t(in[o + 2]) << 16)
			| (std::uint32_t(in[o + 3]) << 24);
		o += 4;
		return true;
	};
	std::uint32_t fmt = 0, nv = 0, tb = 0, dl = 0;
	if (!read_u32(fmt) || fmt != 1 || !read_u32(nv) || !read_u32(tb) || !read_u32(dl)) {
		error_out = "nvram blob bad header";
		return false;
	}
	if (o + std::size_t(nv) + std::size_t(tb) + std::size_t(dl) != in.size()) {
		error_out = "nvram blob size mismatch";
		return false;
	}
	m_nvram.assign(in.begin() + std::ptrdiff_t(o), in.begin() + std::ptrdiff_t(o + nv));
	o += nv;
	m_table.assign(in.begin() + std::ptrdiff_t(o), in.begin() + std::ptrdiff_t(o + tb));
	o += tb;
	m_dla.assign(in.begin() + std::ptrdiff_t(o), in.begin() + std::ptrdiff_t(o + dl));
	if (m_layout.nvram_size != 0 && m_nvram.size() != m_layout.nvram_size) {
		error_out = "persisted NVRAM size does not match board profile";
		return false;
	}
	if (m_layout.table_storage_size != 0 && m_table.size() != m_layout.table_storage_size) {
		error_out = "persisted table storage size does not match board profile";
		return false;
	}
	if (m_layout.dla_stage_size != 0 && m_dla.size() != m_layout.dla_stage_size) {
		error_out = "persisted DLA staging size does not match board profile";
		return false;
	}
	m_checksum_failure = false;
	m_use_sum8_envelope = true;
	m_envelope_sum8 = compute_sum8(m_nvram);
	return true;
}
