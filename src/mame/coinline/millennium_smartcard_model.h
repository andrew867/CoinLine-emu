// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

struct millennium_smart_fixture {
	std::string protocol = "memory";
	std::vector<std::uint8_t> atr{};
	std::vector<std::uint8_t> memory{};
	unsigned atr_delay_us = 4000;
	unsigned apdu_response_delay_us = 1000;
	bool requires_authorization = false;
	// Optional, bounded test memory programming. The contact-test card is read-only.
	bool sync_program_enabled = false;
	unsigned sync_program_first_bit = 0;
	unsigned sync_program_end_bit = 0; // exclusive
	unsigned sync_program_us = 10000;
};

class millennium_smartcard_model {
public:
	enum class negotiated_protocol : std::uint8_t {
		none = 0,
		t0,
		t1,
		sync_fallback,
	};

	bool parse_fixture_json(std::string const &json_text, std::string &error_out);

	void reset_session();
	void insert_card_at(std::uint64_t cycle, std::uint64_t cpu_hz);
	void remove_card();

	void notify_reset(std::uint64_t cycle);
	std::uint8_t status_lines() const;

	// Synchronous contact-card interface. The board drives real power,
	// reset and clock pins; the card exposes one bit, not a byte FIFO.
	void update_later_pins(std::uint8_t port_d, std::uint8_t port_e, std::uint8_t port_f, std::uint64_t cycle = 0);
	void update_earlier_pins(std::uint8_t port_a, std::uint8_t port_c, std::uint64_t cycle = 0);
	bool sync_data_selected() const noexcept { return m_sync_selected; }
	bool sync_data_high() const noexcept;
	bool sync_powered() const noexcept { return m_sync_powered; }
	std::size_t sync_bit_address() const noexcept { return m_sync_bit_address; }

	std::uint8_t read_fifo(std::uint64_t cycle);
	void write_command(std::uint8_t b, std::uint64_t cycle);
	bool negotiate_protocol(bool allow_sync_probe);
	void authorize_session(bool authorized) noexcept { m_authorized = authorized; }

	std::size_t atr_bytes_remaining() const noexcept { return m_atr_pending.size(); }
	std::vector<std::uint8_t> const &memory() const noexcept { return m_fixture.memory; }
	std::string const &protocol() const noexcept { return m_fixture.protocol; }
	negotiated_protocol selected_protocol() const noexcept { return m_selected_protocol; }

private:
	void schedule_atr(std::uint64_t cycle);
	void drive_sync_pins(bool selected, bool power, bool reset, bool clock, bool released, std::uint64_t cycle);
	void clear_sync_session();
	void finish_sync_program(std::uint64_t cycle);

	millennium_smart_fixture m_fixture{};
	std::deque<std::uint8_t> m_rx{};
	std::vector<std::uint8_t> m_atr_pending{};
	bool m_card_present = false;
	bool m_reset_pending = false;
	std::uint64_t m_atr_ready_cycle = 0;
	std::uint64_t m_block_until_cycle = 0;
	bool m_atr_enqueued = false;
	std::uint64_t m_cpu_hz = 12288000;
	unsigned m_read_addr = 0;
	bool m_cmd_phase = false;
	std::uint8_t m_cmd0 = 0;
	negotiated_protocol m_selected_protocol = negotiated_protocol::none;
	bool m_authorized = false;
	bool m_sync_selected = false;
	bool m_sync_powered = false;
	bool m_sync_reset = false;
	bool m_sync_clock = false;
	bool m_sync_released = true;
	bool m_sync_reset_clocked = false;
	bool m_sync_ready = false;
	bool m_later_power_latch = false;
	bool m_later_power_clock = false;
	std::size_t m_sync_bit_address = 0;
	bool m_sync_program_armed = false;
	bool m_sync_program_active = false;
	std::uint64_t m_sync_program_start = 0;
};
