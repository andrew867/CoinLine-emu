// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>

/// Microwire serial EEPROM compatible with Microchip 93LC66 (4 Kbit = 512×8 or 256×16).
/// Serial EEPROM. The board driver reports chip-select, each SK rising edge, and the DI bit.
/// DO is read back on the status port while chip-select is asserted.
///
/// Bits shift on SK rising edges while CS is high. A clock pulse drives low, then high, then low;
/// DI must be valid before the rising edge.
class millennium_microwire_93c66 {
public:
	static constexpr std::size_t k_capacity_bytes = 512U;

	millennium_microwire_93c66() { reset(); }

	void reset();

	void load_from_span(std::uint8_t const *src, std::size_t bytes);
	void store_to_span(std::uint8_t *dst, std::size_t bytes) const;

	/// Chip-select from the control latch, bit 6.
	void set_chip_select(bool cs_high);

	/// Later motherboard: SK is PIO B bit 7. \p di_from_port_a_bit0 is PIO A bit 0.
	void notify_port_b_clock(std::uint8_t prev_b, std::uint8_t new_b, bool di_from_port_a_bit0);

	/// SK level change. Rising edge while CS is high shifts DI.
	void notify_sk_edge(bool prev_sk_high, bool new_sk_high, bool di_bit);

	/// Command field length after START: two opcode bits and either six or eight address bits.
	void set_command_body_bits(unsigned bits);

	/// Current DO line (high = pull-up / logical 1) while CS asserted.
	bool serial_out() const noexcept { return m_do; }

	bool chip_select() const noexcept { return m_cs; }

	/// Optional: mirror completed writes into NVRAM model (offsets within chip).
	void set_write_mirror_cb(std::function<void(std::uint32_t byte_offset, std::uint16_t word_be)> cb)
	{
		m_write_cb = std::move(cb);
	}

	/// Optional JSONL: \p op is one of \c ewen, \c ewds, \c erase_all, \c erase_word, \c read, \c write.
	void set_access_trace_cb(std::function<void(char const *op, std::uint16_t word_addr, std::uint16_t word_be)> cb)
	{
		m_access_trace_cb = std::move(cb);
	}

private:
	void trace_access(char const *op, std::uint16_t word_addr, std::uint16_t word_be)
	{
		if (m_access_trace_cb)
			m_access_trace_cb(op, word_addr, word_be);
	}

	enum class phase : std::uint8_t {
		idle,
		wait_start,
		command_stream,
		read_word_out,
		write_word_in,
	};

	void accept_command();
	void begin_read_output(std::uint16_t word_addr);
	void finish_write_word(std::uint16_t word_addr, std::uint16_t data_be);
	void perform_erase_all_words();
	static std::uint16_t read_word_le(std::array<std::uint8_t, k_capacity_bytes> const &mem, std::uint16_t word_addr);
	static void write_word_le(std::array<std::uint8_t, k_capacity_bytes> &mem, std::uint16_t word_addr,
		std::uint16_t data_be);

	std::array<std::uint8_t, k_capacity_bytes> m_mem{};
	bool m_cs = false;
	bool m_do = true;
	bool m_prev_sk = false;
	bool m_write_enabled = false;

	phase m_phase = phase::idle;
	unsigned m_command_body_bits = 10U;
	std::uint32_t m_sr_bits = 0; // command_stream: START seeds bit 0, then m_command_body_bits DI bits
	unsigned m_sr_len = 0;
	std::uint16_t m_word_addr = 0;
	unsigned m_read_bit_idx = 0; // read_word_out: SK rises completed in the data phase (0..17)
	unsigned m_write_shift = 0;
	std::uint16_t m_write_accum = 0;
	/// Captured when a WRITE opcode completes: only \c true if EWEN (or equivalent) was active.
	bool m_write_commit = false;

	std::function<void(std::uint32_t, std::uint16_t)> m_write_cb;
	std::function<void(char const *, std::uint16_t, std::uint16_t)> m_access_trace_cb;
};
