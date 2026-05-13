// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>

/// Microwire serial EEPROM compatible with Microchip 93LC66 (4 Kbit = 512×8 or 256×16).
/// Wired per Millennium board decode: EEPROM_CS on HW_CNTL (\c 0x40 write), EEPROM_SK on PIO B bit 7,
/// EEPROM_DI on PIO A bit 0, EEPROM_DO sampled from board status port bit 3 on reads.
///
/// This model shifts command/data on SK **rising** edges while CS is asserted (matches the host
/// toggling: pulse drives low→high→low; DI must be valid before the rising edge).
class millennium_microwire_93c66 {
public:
	static constexpr std::size_t k_capacity_bytes = 512U;

	millennium_microwire_93c66() { reset(); }

	void reset();

	void load_from_span(std::uint8_t const *src, std::size_t bytes);

	/// CS input directly from HW_CNTL_PORT latch bit 6 (0x40).
	void set_chip_select(bool cs_high);

	/// Call after PIO port B write with previous image for edge detection.
	void notify_port_b_clock(std::uint8_t prev_b, std::uint8_t new_b, bool di_from_port_a_bit0);

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

	void append_command_bit(bool di);
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
	std::uint32_t m_sr_bits = 0; // command_stream: Microwire SR (START seeds bit 0, then 11 DI bits)
	unsigned m_sr_len = 0;
	std::uint16_t m_word_addr = 0;
	unsigned m_read_bit_idx = 0; // read_word_out: SK rises completed in data phase (0..17, host read sequence)
	unsigned m_write_shift = 0;
	std::uint16_t m_write_accum = 0;
	/// Captured when a WRITE opcode completes: only \c true if EWEN (or equivalent) was active.
	bool m_write_commit = false;

	std::function<void(std::uint32_t, std::uint16_t)> m_write_cb;
	std::function<void(char const *, std::uint16_t, std::uint16_t)> m_access_trace_cb;
};
