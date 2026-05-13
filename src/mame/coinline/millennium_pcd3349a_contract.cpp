// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_pcd3349a_contract.h"

#include <algorithm>
#include <cstdlib>
#include <utility>

namespace {

constexpr std::uint32_t k_terminal21_hook_bit = 0x00080000U;
constexpr std::uint32_t k_rep_dial_6_to_10_mask =
	0x02000000U | 0x04000000U | 0x08000000U | 0x10000000U | 0x20000000U;

std::uint32_t terminal21_keymatrix_applied_mask(millennium_terminal21_user_io_profile p) noexcept
{
	return (p == millennium_terminal21_user_io_profile::repdial_5) ? ~k_rep_dial_6_to_10_mask : ~0U;
}

/** Single TP→CP wire byte from the harness keymatrix (priority matches \c tp_process_front_panel_events). */
std::uint8_t resolve_terminal21_wire_opcode(std::uint32_t keymatrix,
	millennium_terminal21_user_io_profile profile,
	std::uint16_t softkeys) noexcept
{
	std::uint32_t const km = keymatrix & terminal21_keymatrix_applied_mask(profile);
	std::uint32_t const keys_down = km & ~k_terminal21_hook_bit;

	static constexpr struct {
		std::uint32_t m;
		std::uint8_t op;
	} tbl[] = {
		{0x00000001U, 0x22}, {0x00000002U, 0x24}, {0x00000004U, 0x26}, {0x00000008U, 0x28},
		{0x00000010U, 0x2A}, {0x00000020U, 0x2C}, {0x00000040U, 0x2E}, {0x00000080U, 0x30},
		{0x00000100U, 0x32}, {0x00000400U, 0x34}, {0x00000200U, 0x36}, {0x00000800U, 0x38},
		{0x40000000U, 0x20}, {0x00001000U, 0x58}, {0x00040000U, 0x5A},
		{0x00002000U, 0x3A}, {0x00004000U, 0x3C}, {0x00008000U, 0x3E},
		{0x00010000U, 0x56}, {0x00020000U, 0x54},
	};
	for (auto const &e : tbl) {
		if ((keys_down & e.m) != 0U)
			return e.op;
	}

	static constexpr std::uint32_t k_rep[10] = {
		0x00100000U, 0x00200000U, 0x00400000U, 0x00800000U, 0x01000000U,
		0x02000000U, 0x04000000U, 0x08000000U, 0x10000000U, 0x20000000U,
	};
	for (unsigned i = 0U; i < 10U; ++i) {
		if ((keys_down & k_rep[i]) != 0U)
			return static_cast<std::uint8_t>(0x40U + unsigned(i) * 2U);
	}

	if (profile == millennium_terminal21_user_io_profile::vfd_11line_softkeys) {
		std::uint16_t const sk = static_cast<std::uint16_t>(softkeys & 0x0fffU);
		if (sk != 0U && (sk & static_cast<std::uint16_t>(sk - 1U)) == 0U) {
			for (unsigned bi = 0U; bi < 12U; ++bi) {
				if (((sk >> bi) & 1U) != 0U)
					return static_cast<std::uint8_t>(0x90U + bi);
			}
		}
	}

	return 0xffU;
}

} // namespace

namespace coinline::tp8048 {

pcd3349a_contract::pcd3349a_contract(am8048_core_contract &core)
	: m_core(core)
{
	auto const parse_mask = [](char const *name, std::uint8_t fallback) -> std::uint8_t {
		char const *v = std::getenv(name);
		if (!v || !*v)
			return fallback;
		char *end = nullptr;
		long parsed = std::strtol(v, &end, 0);
		if (end == v)
			return fallback;
		return static_cast<std::uint8_t>(parsed & 0xffL);
	};
	m_port_reset_mask[0] = parse_mask("COINLINE_TP8048_PORT0_RESET", 0xffU);
	m_port_reset_mask[1] = parse_mask("COINLINE_TP8048_PORT1_RESET", 0xffU);
	m_port_reset_mask[2] = parse_mask("COINLINE_TP8048_PORT2_RESET", 0xffU);

	am8048_config cfg{};
	am8048_callbacks cb{};
	cb.read_port = [this](unsigned p) { return read_port(p); };
	cb.write_port = [this](unsigned p, std::uint8_t v) { write_port(p, v); };
	// External program memory (MOVX); PCD3349A firmware path uses port I/O; leave empty.
	cb.read_test_input = [this](unsigned i) { return read_test_input(i); };
	cb.tone_registers_changed = [this](std::uint8_t hgf, std::uint8_t lgf) { tone_registers_changed(hgf, lgf); };
	cb.trace = [this](char const *e, std::uint64_t c, std::uint8_t v) { trace(e, c, v); };
	m_core.configure(cfg, std::move(cb));
}

void pcd3349a_contract::reset()
{
	m_tx_bytes.clear();
	m_ui_events.clear();
	m_obs = tp_observable_state{};
	m_obs.state_name = "reset_pending";
	m_obs.hgf = 0U;
	m_obs.lgf = 0U;
	m_obs.tone = tp_tone_mode::none;
	m_last_step_cp_cycle = 0ULL;
	m_port_bus_in = 0xffU;
	m_port_p1_in = 0xffU;
	m_port_p2_in = 0xffU;
	m_port_latch = m_port_reset_mask;
	m_cp_rx_bytes.clear();
	m_servicing_cp_byte = false;
	m_relay_cp_seized = false;
	m_core.reset();
	// First POWER_ON_* byte comes from TP program ROM (QUEUE_BOOT_ACK), not injected here.
	m_obs.tx_pending = false;
	m_obs.state_name = "reset_pending";
}

void pcd3349a_contract::load_firmware_rom(std::uint8_t const *data, std::size_t size)
{
	m_core.load_program_rom(data, size);
}

void pcd3349a_contract::set_input_snapshot(tp_input_snapshot const &input)
{
	m_input = input;
	m_obs.hook_off = (input.keymatrix & 0x00080000U) != 0U;
	m_obs.line_ok = (input.linectrl & 0x01U) != 0U;

	bool const off_hook = (input.keymatrix & 0x00080000U) != 0U;
	std::uint8_t const linectrl = static_cast<std::uint8_t>(input.linectrl & 0xffU);

	std::uint8_t const wire =
		resolve_terminal21_wire_opcode(input.keymatrix, input.terminal_21_profile, input.softkeys);

	std::uint8_t const host_p2 =
		static_cast<std::uint8_t>((input.oos_visible ? 0x40U : 0U) | (input.voice_active ? 0x80U : 0U));

	// Packed wire opcode: P1.6=1, P1[5:0]=low 6 bits, P2[5:4]=bits 7..6 of opcode (softkeys 0x90.. need this).
	// P2.6/P2.7: host OOS / voice-active (behavioural ROM SAMPLE_HOST_FLAGS — dial tone is not synthesized here).
	// P1.7 high = on-hook (idle handset), clear when off-hook — matches SAMPLE_HOOK in behavioural ROM.
	if (wire != 0xffU) {
		m_port_p1_in = static_cast<std::uint8_t>((off_hook ? 0U : 0x80U) | 0x40U | (wire & 0x3fU));
		m_port_p2_in = static_cast<std::uint8_t>(
			((linectrl & 0xcfU) | (((wire >> 6) & 3U) << 4)) | host_p2);
		update_tone_mode();
		return;
	}

	// Legacy nibble path: low nibble indexes DIGTAB (digit 0 uses bit 0x400 per terminal_21 harness).
	std::uint8_t p1 = 0x8fU;
	if (off_hook)
		p1 = static_cast<std::uint8_t>(p1 & ~0x80U);
	auto const map_key_nibble = [](std::uint32_t km) -> std::uint8_t {
		if (km & 0x00000400U)
			return 0x00U;
		if (km & 0x00000001U)
			return 0x01U;
		if (km & 0x00000002U)
			return 0x02U;
		if (km & 0x00000004U)
			return 0x03U;
		if (km & 0x00000008U)
			return 0x04U;
		if (km & 0x00000010U)
			return 0x05U;
		if (km & 0x00000020U)
			return 0x06U;
		if (km & 0x00000040U)
			return 0x07U;
		if (km & 0x00000080U)
			return 0x08U;
		if (km & 0x00000100U)
			return 0x09U;
		return 0x0fU;
	};
	p1 = static_cast<std::uint8_t>((p1 & 0xf0U) | map_key_nibble(input.keymatrix));
	m_port_p1_in = p1;
	m_port_p2_in = static_cast<std::uint8_t>((linectrl & 0x3fU) | host_p2);
	update_tone_mode();
}

void pcd3349a_contract::receive_cp_byte(std::uint8_t byte)
{
	m_obs.last_cp_opcode = byte;
	// TP→CP hook relay: \c 0x3C release, \c 0x3D seize — CP ownership of hook relay for audio path.
	if (byte == 0x3cU)
		m_relay_cp_seized = false;
	else if (byte == 0x3dU)
		m_relay_cp_seized = true;
	m_cp_rx_bytes.push_back(byte);
	if (byte == 0x3cU || byte == 0x3dU)
		update_tone_mode();
}

void pcd3349a_contract::run_until_cp_cycle(std::uint64_t cp_cycle, std::uint32_t cp_hz)
{
	if (cp_cycle <= m_last_step_cp_cycle)
		return;
	std::uint64_t const delta_cp = cp_cycle - m_last_step_cp_cycle;
	std::uint64_t const tp_hz = 3579545ULL;
	std::uint64_t const tp_cycles = (delta_cp * tp_hz) / std::max<std::uint32_t>(1U, cp_hz);
	// Inject pending CP->TP bytes via BUS and external interrupt.
	while (!m_cp_rx_bytes.empty()) {
		m_port_bus_in = m_cp_rx_bytes.front();
		m_cp_rx_bytes.pop_front();
		m_servicing_cp_byte = true;
		m_core.assert_external_interrupt(true);
		m_core.step_cycles(32U);
		m_core.assert_external_interrupt(false);
		m_servicing_cp_byte = false;
	}
	if (tp_cycles > 0U)
		m_core.step_cycles(tp_cycles);
	m_last_step_cp_cycle = cp_cycle;
	m_obs.tp_cycles = m_core.cycles();
}

bool pcd3349a_contract::has_tx_byte() const
{
	return !m_tx_bytes.empty();
}

std::uint8_t pcd3349a_contract::pop_tx_byte()
{
	std::uint8_t const v = m_tx_bytes.front();
	m_tx_bytes.pop_front();
	m_obs.tx_pending = !m_tx_bytes.empty();
	return v;
}

bool pcd3349a_contract::has_ui_event() const
{
	return !m_ui_events.empty();
}

std::uint8_t pcd3349a_contract::pop_ui_event()
{
	std::uint8_t const v = m_ui_events.front();
	m_ui_events.pop_front();
	return v;
}

tp_observable_state pcd3349a_contract::observable() const
{
	tp_observable_state o = m_obs;
	o.relay_cp_seized = m_relay_cp_seized;
	return o;
}

std::uint8_t pcd3349a_contract::read_port(unsigned port)
{
	auto const quasi_read = [](std::uint8_t latch, std::uint8_t ext) -> std::uint8_t {
		// Quasi-bidirectional behavior: latched zero drives low, latched one allows external value.
		return static_cast<std::uint8_t>(latch & ext);
	};
	if (port == 0U) return m_port_bus_in;
	if (port == 1U) return quasi_read(m_port_latch[1], m_port_p1_in);
	if (port == 2U) return quasi_read(m_port_latch[2], m_port_p2_in);
	return 0xffU;
}

void pcd3349a_contract::write_port(unsigned port, std::uint8_t value)
{
	if (port < m_port_latch.size())
		m_port_latch[port] = value;
	if (port == 0U && m_servicing_cp_byte) {
		m_tx_bytes.push_back(value);
		m_obs.tx_pending = true;
	} else if (port == 0U) {
		// TP8048 firmware drives TP→CP keypad/hook bytes via P0; the host does not synthesize or
		// second-guess wire opcodes. Drop only open-bus idle (never a catalogued TP→CP single byte).
		if (value == 0xffU)
			return;
		m_ui_events.push_back(value);
	} else if (port == 4U) {
		m_obs.hgf = value;
		m_obs.tone = (m_obs.hgf == 0U && m_obs.lgf == 0U) ? tp_tone_mode::none : tp_tone_mode::dtmf;
	} else if (port == 5U) {
		m_obs.lgf = value;
		m_obs.tone = (m_obs.hgf == 0U && m_obs.lgf == 0U) ? tp_tone_mode::none : tp_tone_mode::dtmf;
	}
}

bool pcd3349a_contract::read_test_input(unsigned input)
{
	if (input == 0U) // CE/T0
		return (m_input.linectrl & 0x02U) != 0U;
	if (input == 1U) // T1
		return (m_input.linectrl & 0x04U) != 0U;
	return false;
}

void pcd3349a_contract::tone_registers_changed(std::uint8_t hgf, std::uint8_t lgf)
{
	m_obs.hgf = hgf;
	m_obs.lgf = lgf;
	m_obs.tone = (hgf == 0U && lgf == 0U) ? tp_tone_mode::none : tp_tone_mode::dtmf;
}

void pcd3349a_contract::trace(char const *, std::uint64_t, std::uint8_t)
{
}

void pcd3349a_contract::update_tone_mode()
{
	if (!m_obs.hook_off) {
		m_obs.tone = tp_tone_mode::none;
		return;
	}
	if (m_input.voice_active) {
		m_obs.tone = tp_tone_mode::none;
		return;
	}
	// OOS SIT/NIS in earpiece: host signals oos_visible; CP SEIZE (0x3D) stops local NIS (relay under CP control).
	// Dial tone is produced elsewhere (voiceware); not modeled as TP tone intent here.
	if (m_input.oos_visible && !m_relay_cp_seized)
		m_obs.tone = tp_tone_mode::nis;
	else
		m_obs.tone = tp_tone_mode::none;
}

} // namespace coinline::tp8048
