// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "millennium_am8048_core_contract.h"
#include "millennium_keypad_model.h"

#include <cstdint>
#include <deque>
#include <string>
#include <array>

namespace coinline::tp8048 {

enum class tp_tone_mode : std::uint8_t { none, dialtone, nis, dtmf };

struct tp_input_snapshot {
	std::uint32_t keymatrix = 0U;
	std::uint8_t linectrl = 0xffU;
	std::uint16_t softkeys = 0xffffU;
	std::uint8_t secmask = 0xffU;
	bool oos_visible = false;
	bool voice_active = false;
	std::uint64_t cp_cycle = 0ULL;
	/// Harness profile (rep-dial row count, VFD softkeys). Drives wire-opcode resolution for the 8048 ROM.
	millennium_terminal21_user_io_profile terminal_21_profile = millennium_terminal21_user_io_profile::repdial_10;
};

struct tp_observable_state {
	bool ready = false;
	bool fault_latched = false;
	bool hook_off = false;
	bool line_ok = true;
	bool tx_pending = false;
	/// CP has issued SEIZE_HOOK_SWITCH_RELAY (0x3D); clears on RELEASE (0x3C). Gates OOS SIT/NIS in earpiece model.
	bool relay_cp_seized = false;
	tp_tone_mode tone = tp_tone_mode::none;
	std::uint64_t tp_cycles = 0ULL;
	std::uint8_t last_cp_opcode = 0U;
	std::string state_name = "reset_pending";
	std::uint8_t hgf = 0U;
	std::uint8_t lgf = 0U;
};

class pcd3349a_contract {
public:
	explicit pcd3349a_contract(am8048_core_contract &core);

	void reset();
	void load_firmware_rom(std::uint8_t const *data, std::size_t size);
	void set_input_snapshot(tp_input_snapshot const &input);
	void receive_cp_byte(std::uint8_t byte);
	void run_until_cp_cycle(std::uint64_t cp_cycle, std::uint32_t cp_hz);
	/** Advance TP core only (does not move CP-cycle coupling baseline). Boot-time primer. */
	void step_tp_cycles_raw(std::uint64_t tp_cycles) { m_core.step_cycles(tp_cycles); }

	bool has_tx_byte() const;
	std::uint8_t pop_tx_byte();
	bool has_ui_event() const;
	std::uint8_t pop_ui_event();
	tp_observable_state observable() const;
	bool relay_cp_seized() const noexcept { return m_relay_cp_seized; }

	char const *backend_id_string() const { return "pcd3349a_8048"; }

private:
	am8048_core_contract &m_core;
	tp_input_snapshot m_input{};
	std::deque<std::uint8_t> m_tx_bytes;
	std::deque<std::uint8_t> m_ui_events;
	tp_observable_state m_obs{};
	std::uint64_t m_last_step_cp_cycle = 0ULL;
	std::uint8_t m_port_bus_in = 0xffU;
	std::uint8_t m_port_p1_in = 0xffU;
	std::uint8_t m_port_p2_in = 0xffU;
	std::array<std::uint8_t, 3> m_port_latch{ 0xffU, 0xffU, 0xffU };
	std::array<std::uint8_t, 3> m_port_reset_mask{ 0xffU, 0xffU, 0xffU };
	std::deque<std::uint8_t> m_cp_rx_bytes;
	bool m_servicing_cp_byte = false;
	/// Hook relay seize/release opcodes — stops OOS NIS when CP seizes the hook relay.
	bool m_relay_cp_seized = false;

	std::uint8_t read_port(unsigned port);
	void write_port(unsigned port, std::uint8_t value);
	bool read_test_input(unsigned input);
	void tone_registers_changed(std::uint8_t hgf, std::uint8_t lgf);
	void trace(char const *event, std::uint64_t cycle, std::uint8_t value);

	void update_tone_mode();
};

} // namespace coinline::tp8048
