// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_microwire_93c66.h"

#include <cstring>

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

void millennium_microwire_93c66::store_to_span(std::uint8_t *dst, std::size_t bytes) const
{
	if (!dst || bytes == 0U)
		return;
	std::memcpy(dst, m_mem.data(), std::min(bytes, k_capacity_bytes));
}

std::uint16_t millennium_microwire_93c66::read_word_le(std::array<std::uint8_t, k_capacity_bytes> const &mem,
	std::uint16_t word_addr)
{
	std::uint16_t const base = static_cast<std::uint16_t>(word_addr & 0xffU) * 2U;
	// High byte is stored first — big-endian 16-bit word in each byte pair.
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

void millennium_microwire_93c66::set_command_body_bits(unsigned bits)
{
	m_command_body_bits = (bits == 8U) ? 8U : 10U;
}

void millennium_microwire_93c66::accept_command()
{
	// The START bit is followed by the two-bit opcode and the selected word address.
	unsigned const address_bits = m_command_body_bits - 2U;
	std::uint8_t const op = static_cast<std::uint8_t>((m_sr_bits >> address_bits) & 3U);
	std::uint8_t const addr = static_cast<std::uint8_t>(m_sr_bits & ((1U << address_bits) - 1U));
	if (op == 0U) {
		std::uint8_t const sub = static_cast<std::uint8_t>((addr >> (address_bits - 2U)) & 3U);
		if (sub == 3U) {
			m_write_enabled = true;
			trace_access("ewen", 0, 0);
		} else if (sub == 2U) {
			if (m_write_enabled)
				perform_erase_all_words();
		} else if (sub == 0U) {
			m_write_enabled = false;
			trace_access("ewds", 0, 0);
		}
		m_phase = phase::wait_start;
		m_do = true;
		return;
	}
	if (op == 2U) {
		begin_read_output(addr);
		return;
	}
	if (op == 1U) {
		m_phase = phase::write_word_in;
		m_write_shift = 0;
		m_write_accum = 0;
		m_write_commit = m_write_enabled;
		m_word_addr = addr;
		return;
	}
	if (op == 3U && m_write_enabled) {
		write_word_le(m_mem, addr, 0xffffU);
		trace_access("erase_word", addr, 0xffffU);
		if (m_write_cb)
			m_write_cb(static_cast<std::uint32_t>(addr) * 2U, read_word_le(m_mem, addr));
	}
	m_phase = phase::wait_start;
	m_do = true;
}

void millennium_microwire_93c66::notify_port_b_clock(std::uint8_t prev_b, std::uint8_t new_b, bool di_bit0)
{
	// Later motherboard clocks on PIO B bit 7.
	notify_sk_edge((prev_b & 0x80U) != 0, (new_b & 0x80U) != 0, di_bit0);
}

void millennium_microwire_93c66::notify_sk_edge(bool prev_sk, bool new_sk, bool di_bit0)
{
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
		// START latches the initial bit before the opcode and address.
		m_sr_bits = 1U;
		m_sr_len = 1U;
		return;
	case phase::command_stream:
		m_sr_bits = (m_sr_bits << 1) | (di_bit0 ? 1U : 0U);
		++m_sr_len;
		if (m_sr_len == 1U + m_command_body_bits)
			accept_command();
		return;
	case phase::read_word_out: {
		// Read is 17 SK rising edges: one dummy bit, then 16 data bits. The status port is
		// sampled before each edge, so a rising edge publishes the next DO bit. The 17th
		// edge completes the word (the last sample was the LSB, taken before that edge).
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
	m_do = false; // Dummy zero precedes the 16 data bits.
}

void millennium_microwire_93c66::finish_write_word(std::uint16_t word_addr, std::uint16_t data_be)
{
	write_word_le(m_mem, word_addr, data_be);
	trace_access("write", word_addr, data_be);
	if (m_write_cb)
		m_write_cb(static_cast<std::uint32_t>(word_addr) * 2U, data_be);
}
