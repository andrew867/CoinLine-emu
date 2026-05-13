// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_microwire_93c66.h"

#include "millennium_eprm_command.h"

#include <cstring>

namespace {

constexpr unsigned k_body_bits = 11; // Microwire command field after START: 10 DI bits + 1 implicit-zero bit

// Eleven-bit command bodies (after the leading start bit) produced by the host's Microwire
// bit-bang sequence: EWEN/EWDS/ERAL use fixed opcode-high-nibble patterns merged with address
// bits; the constants below are the wire images observed for C = 0 erase/write-setup paths.
constexpr std::uint16_t k_cmd_body_ewen = 0x180U; // write enable
constexpr std::uint16_t k_cmd_body_ewds = 0x000U; // write disable
constexpr std::uint16_t k_cmd_body_erase_all = 0x100U; // erase entire array

// Erase/write-enable opcode high nibble = 0x30; the on-wire 11-bit body is
// 0x400 | ((body11 >> 1) & 0x3FF) with body11 produced by millennium_eprm_command_body11 —
// typically 0x4C0..0x4FF depending on merged address bits (not 0x44a-only).
bool microwire_wire_body_is_eprm_ewen(std::uint16_t body)
{
	for (unsigned c = 0; c < 256U; ++c) {
		std::uint16_t const b11 = millennium_eprm_command_body11(0x30U, static_cast<std::uint8_t>(c));
		std::uint16_t const wire = static_cast<std::uint16_t>(0x400U | ((b11 >> 1) & 0x3FFU));
		if (wire == body)
			return true;
	}
	return false;
}

} // namespace

void millennium_microwire_93c66::reset()
{
	m_mem.fill(0xff);
	m_cs = false;
	m_do = true;
	m_prev_sk = false;
	m_write_enabled = false; // EWEN required before writes (matches 93LC66 power-on / EWDS default).
	m_phase = phase::idle;
	m_sr_bits = 0;
	m_sr_len = 0;
	m_word_addr = 0;
	m_read_bit_idx = 0;
	m_write_shift = 0;
	m_write_accum = 0;
	m_write_commit = false;
}

void millennium_microwire_93c66::load_from_span(std::uint8_t const *src, std::size_t bytes)
{
	m_mem.fill(0xff);
	if (!src || bytes == 0)
		return;
	std::size_t const n = std::min(bytes, k_capacity_bytes);
	std::memcpy(m_mem.data(), src, n);
}

std::uint16_t millennium_microwire_93c66::read_word_le(std::array<std::uint8_t, k_capacity_bytes> const &mem,
	std::uint16_t word_addr)
{
	std::uint16_t const base = static_cast<std::uint16_t>(word_addr & 0xffU) * 2U;
	// Host stores high byte first, then low byte — big-endian 16-bit word in byte pairs.
	std::uint8_t const hi = mem.at(base);
	std::uint8_t const lo = mem.at(static_cast<std::size_t>(base) + 1U);
	return static_cast<std::uint16_t>((unsigned(hi) << 8) | unsigned(lo));
}

void millennium_microwire_93c66::write_word_le(std::array<std::uint8_t, k_capacity_bytes> &mem, std::uint16_t word_addr,
	std::uint16_t data_be)
{
	std::uint16_t const base = static_cast<std::uint16_t>(word_addr & 0xffU) * 2U;
	mem.at(base) = static_cast<std::uint8_t>((data_be >> 8) & 0xffU);
	mem.at(static_cast<std::size_t>(base) + 1U) = static_cast<std::uint8_t>(data_be & 0xffU);
}

void millennium_microwire_93c66::perform_erase_all_words()
{
	trace_access("erase_all", 0, 0);
	for (unsigned w = 0; w < 256U; ++w) {
		std::uint16_t const wa = static_cast<std::uint16_t>(w & 0xffU);
		write_word_le(m_mem, wa, 0xffffU);
		if (m_write_cb)
			m_write_cb(static_cast<std::uint32_t>(wa) * 2U, read_word_le(m_mem, wa));
	}
}

void millennium_microwire_93c66::set_chip_select(bool cs_high)
{
	bool const was = m_cs;
	m_cs = cs_high;
	if (!cs_high && was) {
		// Firmware often drops CS after 16 data SK edges following the dummy bit, without a 17th
		// rising edge; trace completed reads on deassert so JSONL gates see real traffic.
		if (m_phase == phase::read_word_out && m_read_bit_idx >= 16U)
			trace_access("read", m_word_addr, read_word_le(m_mem, m_word_addr));
		m_phase = phase::idle;
		m_sr_bits = 0;
		m_sr_len = 0;
		m_read_bit_idx = 0;
		m_write_shift = 0;
		m_write_accum = 0;
		m_write_commit = false;
		m_do = true;
		m_prev_sk = false;
		return;
	}
	if (cs_high && !was) {
		m_phase = phase::wait_start;
		m_sr_bits = 0;
		m_sr_len = 0;
		m_read_bit_idx = 0;
		m_write_shift = 0;
		m_write_accum = 0;
		m_write_commit = false;
		m_do = true;
	}
}

void millennium_microwire_93c66::notify_port_b_clock(std::uint8_t prev_b, std::uint8_t new_b, bool di_bit0)
{
	bool const prev_sk = (prev_b & 0x80U) != 0;
	bool const new_sk = (new_b & 0x80U) != 0;
	(void)m_prev_sk;
	m_prev_sk = new_sk;
	if (!m_cs)
		return;

	bool const rising = !prev_sk && new_sk;
	if (!rising)
		return;

	switch (m_phase) {
	// CS can stay asserted across multiple 93LC66 commands; after a completed op we return to
	// wait_start (not idle) so the next SK rising edge can begin a new start bit. Using idle here
	// dropped every clock until CS toggled, which prevented multi-command sessions on one CS pulse.
	case phase::idle:
		m_phase = phase::wait_start;
		[[fallthrough]];
	case phase::wait_start:
		if (!di_bit0)
			return;
		m_phase = phase::command_stream;
		// Model matches observed bit-bang: START latches sr=1, then 11 rising edges with
		// body = (sr >> 1) & 0x7FF (opcode in bits [10:8], address in [7:0]). Reference 11-bit
		// images from millennium_eprm_command_body11() relate as: body_wire = 0x400 | (b >> 1).
		m_sr_bits = 1U;
		m_sr_len = 1U;
		return;
	case phase::command_stream: {
		m_sr_bits = (m_sr_bits << 1) | (di_bit0 ? 1U : 0U);
		++m_sr_len;
		if (m_sr_len < 1U + k_body_bits)
			return;

		std::uint16_t const body = static_cast<std::uint16_t>((m_sr_bits >> 1) & ((1U << k_body_bits) - 1U));
		std::uint8_t const opcode = static_cast<std::uint8_t>((body >> 8) & 7U);
		std::uint8_t const addr = static_cast<std::uint8_t>(body & 0xffU);
		m_word_addr = addr;

		// EWEN packs to 0x180 in the reference body11 encoding but never appears as 0x180 on this
		// wire representation (real wire bodies are always >= 0x400); keep the constant check anyway
		// for completeness.
		if (body == k_cmd_body_ewen) {
			m_write_enabled = true;
			trace_access("ewen", 0, 0);
			m_phase = phase::wait_start;
			m_do = true;
			return;
		}
		if (body == 0x44aU || microwire_wire_body_is_eprm_ewen(body)) {
			m_write_enabled = true;
			trace_access("ewen", static_cast<std::uint16_t>(body & 0xffU), body);
			m_phase = phase::wait_start;
			m_do = true;
			return;
		}
		if (body == k_cmd_body_ewds) {
			m_write_enabled = false;
			trace_access("ewds", 0, 0);
			m_phase = phase::wait_start;
			m_do = true;
			return;
		}
		if (body == k_cmd_body_erase_all) {
			if (m_write_enabled)
				perform_erase_all_words();
			m_phase = phase::wait_start;
			m_do = true;
			return;
		}
		if (opcode == 6U) {
			begin_read_output(addr);
			return;
		}
		if (opcode == 5U) {
			m_phase = phase::write_word_in;
			m_write_shift = 0;
			m_write_accum = 0;
			m_write_commit = m_write_enabled;
			return;
		}
		if (opcode == 7U && m_write_enabled) {
			write_word_le(m_mem, addr, 0xffffU);
			trace_access("erase_word", addr, 0xffffU);
			if (m_write_cb)
				m_write_cb(static_cast<std::uint32_t>(addr) * 2U, read_word_le(m_mem, addr));
			m_phase = phase::wait_start;
			return;
		}
		m_phase = phase::wait_start;
		m_do = true;
		return;
	}
	case phase::read_word_out: {
		// Host read sequencer: 17 sample loops — "1 dummy bit, then 16 genuine"; each iteration
		// samples DO then toggles SK. The dummy is present before the first data SK edge (set up
		// in begin_read_output); rising edge k (1..16) clocks DO to bit (16-k) from MSB for the
		// next sample; the 17th rising edge completes the read (last sample was LSB before that edge).
		std::uint16_t const w = read_word_le(m_mem, m_word_addr);
		++m_read_bit_idx;
		if (m_read_bit_idx == 17U) {
			trace_access("read", m_word_addr, read_word_le(m_mem, m_word_addr));
			m_phase = phase::wait_start;
			m_do = true;
			return;
		}
		unsigned const bit_from_msb = 16U - m_read_bit_idx;
		m_do = ((w >> bit_from_msb) & 1U) != 0;
		return;
	}
	case phase::write_word_in:
		m_write_accum = static_cast<std::uint16_t>((m_write_accum << 1) | (di_bit0 ? 1U : 0U));
		++m_write_shift;
		if (m_write_shift >= 16U) {
			if (m_write_commit)
				finish_write_word(m_word_addr, m_write_accum);
			else
				trace_access("write_rejected", m_word_addr, m_write_accum);
			m_phase = phase::wait_start;
			m_write_shift = 0;
			m_write_accum = 0;
			m_write_commit = false;
		}
		return;
	}
}

void millennium_microwire_93c66::begin_read_output(std::uint16_t word_addr)
{
	m_word_addr = word_addr;
	m_phase = phase::read_word_out;
	m_read_bit_idx = 0;
	m_do = true; // dummy bit — first eprm_in_word_lp IN0 runs before the first data SK toggle
}

void millennium_microwire_93c66::finish_write_word(std::uint16_t word_addr, std::uint16_t data_be)
{
	write_word_le(m_mem, word_addr, data_be);
	trace_access("write", word_addr, data_be);
	if (m_write_cb)
		m_write_cb(static_cast<std::uint32_t>(word_addr) * 2U, data_be);
}

void millennium_microwire_93c66::append_command_bit(bool di)
{
	(void)di;
}
