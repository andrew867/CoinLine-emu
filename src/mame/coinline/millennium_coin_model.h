// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct millennium_coin_board_config {
	std::string validator_type = "pulse";
	std::vector<int> denominations_cents = {5, 10, 25, 100};
	unsigned pulse_width_us = 350;
	unsigned inter_pulse_gap_us = 350;
};

/// Pulse train + UART-framed protocol subset (wake, learn-escrow, escrow collect/refund, status poll).
class millennium_coin_model {
public:
	struct protocol_status_frame {
		std::uint8_t code = 0x42U;
		std::uint8_t fault = 0x00U;
		std::uint8_t sensor = 0x00U;
	};

	enum class protocol_command : std::uint8_t {
		poll_status = 0x42,
		reset_fault = 0x52,
		wake = 0x67,
		sleep_mode = 0x73,
		checksum_request = 0x6B,
		enable_acceptance = 0x72,
		collect_escrow = 0x63,
		refund_escrow = 0x66,
		learn_escrow = 0x6C,
	};

	void configure(millennium_coin_board_config const &cfg);
	void reset();

	void write_control(std::uint8_t data, std::uint64_t cycle);
	std::uint8_t read_status(std::uint64_t cycle);

	/// Schedule a validated pulse train for `cents` (must appear in profile denominations).
	bool begin_insert_cents(int cents, std::uint64_t cycle, std::uint64_t cpu_hz);
	void inject_jam(std::uint64_t cycle);
	void inject_reject_route(std::uint64_t cycle);

	std::uint64_t pulse_width_cycles(std::uint64_t cpu_hz) const;
	std::uint64_t inter_pulse_gap_cycles(std::uint64_t cpu_hz) const;

	bool disabled() const noexcept { return m_disabled; }
	bool jammed() const noexcept { return m_jam_latched; }
	int pending_denom_cents() const noexcept { return m_pending_cents; }
	bool escrow_present() const noexcept { return m_escrow_cents > 0 || m_uart_escrow_cents > 0; }

	void set_cpu_hz(std::uint64_t hz) noexcept { m_cpu_hz = hz ? hz : 12288000; }

	// Simplified command/response protocol model for UART-based validator profiles.
	bool protocol_handle_command(protocol_command cmd, protocol_status_frame &status_out) noexcept;
	void protocol_note_timeout() noexcept;
	unsigned protocol_timeout_count() const noexcept { return m_protocol_timeout_count; }
	unsigned protocol_reset_pulse_count() const noexcept { return m_protocol_reset_pulse_count; }
	bool uart_active() const noexcept { return m_uart_active; }
	std::vector<std::uint8_t> uart_command(std::uint8_t byte, std::uint64_t now_ms = 0);
	std::optional<std::uint8_t> uart_insert(int cents, std::uint64_t now_ms = 0);
	void set_cashbox_state(bool present, bool full) noexcept;
	void advance_uart(std::uint64_t now_ms) noexcept;
	struct physical_contents { std::uint64_t escrow = 0, cashbox = 0, refunded = 0, removed = 0; };
	physical_contents contents() const noexcept { return {m_uart_escrow_cents, m_cashbox_cents, m_refunded_cents, m_removed_cents}; }
	void restore_contents(physical_contents const &value) noexcept;
	bool cashbox_present() const noexcept { return m_cashbox_present; }
	bool cashbox_full() const noexcept { return m_cashbox_full; }

private:
	int pulses_for_valid_denom(int cents) const;
	bool denom_allowed(int cents) const;

	millennium_coin_board_config m_cfg{};
	std::uint64_t m_cpu_hz = 12288000;

	bool m_disabled = false;
	bool m_jam_latched = false;
	bool m_reject_route = false;
	bool m_emitting = false;
	int m_pending_cents = 0;
	int m_escrow_cents = 0;
	unsigned m_pulse_target = 0;
	std::uint64_t m_train_start_cycle = 0;
	unsigned m_protocol_timeout_count = 0U;
	unsigned m_protocol_reset_pulse_count = 0U;
	std::uint16_t m_uart_windows = 0xffffU;
	std::uint8_t m_uart_pending_command = 0U;
	unsigned m_uart_address_bytes = 0U;
	std::uint64_t m_uart_escrow_cents = 0;
	std::uint64_t m_cashbox_cents = 0, m_refunded_cents = 0, m_removed_cents = 0;
	bool m_cashbox_present = true, m_cashbox_full = false, m_uart_escrow_fault = false;
	enum class gate_motion { idle, collect, refund, neutral };
	gate_motion m_uart_motion = gate_motion::idle;
	std::uint64_t m_uart_escrow_motion_end_ms = 0;
	bool m_uart_active = false;
	bool m_uart_awake = true;
	bool m_uart_accepting = false;
	bool m_uart_one_coin = false;
};
