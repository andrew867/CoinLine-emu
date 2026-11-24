// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_pcd3349a_contract.h"
#include "millennium_am8048_core.h"

#include <algorithm>
#include <cstdio>
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
	m_boot_tx_window = true;
	m_boot_ack_emitted = false;
	m_suppress_boot_ack_echo = false;
	m_cfg_rx_remaining = 0U;
	m_cfg_ack_pending = false;
	m_relay_cp_seized = false;
	m_p0_tx_latch = 0xffU;
	m_p0_tx_latch_valid = false;
	m_p0_tx_overrun_bytes.clear();
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
	std::uint8_t const linectrl = static_cast<std::uint8_t>(input.linectrl & 0xffU);
	bool const off_hook = (input.keymatrix & k_terminal21_hook_bit) != 0U;
	m_obs.hook_off = off_hook;
	m_obs.line_ok = (input.linectrl & 0x01U) != 0U;

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
	m_boot_tx_window = false;
	m_obs.last_cp_opcode = byte;
	// TP→CP hook relay: \c 0x3C release, \c 0x3D seize — CP ownership of hook relay for audio path.
	if (byte == 0x3cU)
		m_relay_cp_seized = false;
	else if (byte == 0x3dU)
		m_relay_cp_seized = true;
	bool const in_cfg = m_cfg_rx_remaining > 0U;
	if (byte == 0xc0U) {
		m_cfg_rx_remaining = 125U;
	} else if (m_cfg_rx_remaining > 0U) {
		m_cfg_rx_remaining--;
	}
	// Tail is the octet that closes a frame already in progress. A bare opcode also
	// has remaining==0 and must not take the config-finish budget.
	bool const config_frame_tail = in_cfg && m_cfg_rx_remaining == 0U;
	m_port_bus_in = byte;
	m_servicing_cp_byte = true;
	pulse_cp_byte_irq(64U);
	m_servicing_cp_byte = false;
	// Do not force the PC into SERVICE_CP_RX. That RET pops the real stack and the
	// following step executes the ROM image as a straight line. The external IRQ
	// already calls the parser; MAIN_LOOP SERVICE_TX then emits one byte per pass.
	// Config payload octets are stored in the IRQ. Further MAIN_LOOP passes in the
	// same host byte were popping stale RX-ring slots and finishing the frame early,
	// so the ACK was emitted while P0 was still suppressed. Only the closing octet
	// needs a slice long enough for CFG_DO_ACCEPT_FINISH to drain its TX ring.
	std::uint32_t const budget = config_frame_tail ? 65536U : (m_cfg_rx_remaining > 0U ? 0U : 16384U);
	m_core.step_cycles(budget);
	commit_p0_tx_latch();
	drain_tx_ring();
	if (char const *trace_path = std::getenv("COINLINE_TP8048_CFG_TRACE")) {
		if (*trace_path) {
			auto const *core = dynamic_cast<millennium_am8048_core const *>(&m_core);
			if (core && (byte == 0xc0U || m_cfg_rx_remaining > 120U || m_cfg_rx_remaining < 5U || config_frame_tail)) {
				if (std::FILE *f = std::fopen(trace_path, "ab")) {
					std::fprintf(f,
						"{\"cp_byte\":\"0x%02X\",\"host_cfg_remaining\":%u,\"tail\":%s,"
						"\"pc\":\"0x%04X\",\"rx_head\":\"0x%02X\",\"rx_tail\":\"0x%02X\","
						"\"cfg_need\":\"0x%02X\",\"cfg_idx\":\"0x%02X\","
						"\"cfg_len\":\"0x%02X\",\"cfg_chk\":\"0x%02X\",\"tmp_sum\":\"0x%02X\","
						"\"tx_head\":\"0x%02X\",\"tx_tail\":\"0x%02X\","
						"\"latched\":%s,\"tx_queue\":%zu}\n",
						unsigned(byte), m_cfg_rx_remaining, config_frame_tail ? "true" : "false",
						unsigned(core->debug_pc() & 0xffffU), unsigned(core->debug_ram(0x20U)),
						unsigned(core->debug_ram(0x21U)), unsigned(core->debug_ram(0x37U)),
						unsigned(core->debug_ram(0x38U)), unsigned(core->debug_ram(0x61U)),
						unsigned(core->debug_ram(0xddU)), unsigned(core->debug_ram(0x3dU)), unsigned(core->debug_ram(0x22U)),
						unsigned(core->debug_ram(0x23U)), m_p0_tx_latch_valid ? "true" : "false",
						m_tx_bytes.size());
					std::fclose(f);
				}
			}
		}
	}
	if (byte == 0x3cU || byte == 0x3dU)
		update_tone_mode();
}

void pcd3349a_contract::drain_tx_ring()
{
	auto *core = dynamic_cast<millennium_am8048_core *>(&m_core);
	if (!core)
		return;
	// SERVICE_TX emits one queued byte per MAIN_LOOP pass via OUTL BUS. A short IRQ
	// window cannot cover that, so keep stepping until TX_HEAD catches TX_TAIL.
	for (unsigned pass = 0U; pass < 512U; ++pass) {
		if (core->debug_ram(0x22U) == core->debug_ram(0x23U))
			break;
		m_core.step_cycles(128U);
	}
	commit_p0_tx_latch();
}

void pcd3349a_contract::flush_boot_tx_queue()
{
	commit_p0_tx_latch();
	// Retire QUEUE_BOOT_ACK's TX ring copy while the pre-link window is still open,
	// so SERVICE_TX cannot echo 0x70/0x72 after the CP query burst starts.
	for (unsigned pass = 0U; pass < 256U && m_boot_tx_window; ++pass) {
		auto const *core = dynamic_cast<millennium_am8048_core const *>(&m_core);
		bool const ring_empty = !core || core->debug_ram(0x22U) == core->debug_ram(0x23U);
		if (m_boot_ack_emitted && ring_empty && !m_suppress_boot_ack_echo)
			break;
		m_core.step_cycles(128U);
		commit_p0_tx_latch();
	}
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
		pulse_cp_byte_irq(32U);
		m_servicing_cp_byte = false;
	}
	if (tp_cycles > 0U)
		m_core.step_cycles(tp_cycles);
	commit_p0_tx_latch();
	drain_tx_ring();
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
	auto const trace_p0_write = [&](char const *action) {
		if (port != 0U)
			return;
		char const *trace_path = std::getenv("COINLINE_TP8048_CFG_TRACE");
		if (!trace_path || !*trace_path)
			return;
		if (std::FILE *f = std::fopen(trace_path, "ab")) {
			auto const *core = dynamic_cast<millennium_am8048_core const *>(&m_core);
			std::fprintf(f,
				"{\"event\":\"p0_write\",\"value\":\"0x%02X\",\"action\":\"%s\","
				"\"host_cfg_remaining\":%u,\"ack_pending\":%s,\"latched\":%s,"
				"\"pc\":\"0x%04X\",\"rx_head\":\"0x%02X\",\"rx_tail\":\"0x%02X\","
				"\"tx_head\":\"0x%02X\",\"tx_tail\":\"0x%02X\",\"hook_seq_rem\":\"0x%02X\","
				"\"hook_seq_byte\":\"0x%02X\"}\n",
				unsigned(value), action ? action : "", m_cfg_rx_remaining,
				m_cfg_ack_pending ? "true" : "false", m_p0_tx_latch_valid ? "true" : "false",
				core ? unsigned(core->debug_pc() & 0xffffU) : 0U,
				core ? unsigned(core->debug_ram(0x20U)) : 0U,
				core ? unsigned(core->debug_ram(0x21U)) : 0U,
				core ? unsigned(core->debug_ram(0x22U)) : 0U,
				core ? unsigned(core->debug_ram(0x23U)) : 0U,
				core ? unsigned(core->debug_ram(0x3fU)) : 0U,
				core ? unsigned(core->debug_ram(0xdeU)) : 0U);
			std::fclose(f);
		}
	};
	if (port == 0U && value == 0xffU) {
		// 0xFF is the quasi-bidirectional idle/open-bus latch value, not a TP wire opcode.
		trace_p0_write("idle_ff");
		return;
	}
	if (port == 0U && m_cfg_rx_remaining > 0U) {
		// CP streams 125 payload bytes after 0xC0; hold TP CSI/O silent until CFG_DO_ACCEPT_FINISH.
		trace_p0_write("suppress_cfg_stream");
		return;
	}
	if (port == 0U && (value == 0x70U || value == 0x72U) && m_suppress_boot_ack_echo) {
		// QUEUE_BOOT_ACK already exposed this byte with OUTL BUS and also queued it.
		// SERVICE_TX would echo it one main-loop pass later. One echo only.
		m_suppress_boot_ack_echo = false;
		trace_p0_write("drop_boot_ack_echo");
		return;
	}
	if (port == 0U && m_boot_tx_window) {
		// The boot primer runs the 8048 before the CP clocks CSI/O. Expose only the
		// ROM-authored POWER_ON_* byte from that window; drop pre-link service chatter.
		if (!m_boot_ack_emitted && (value == 0x70U || value == 0x72U)) {
			note_cp_tx_byte(value);
			m_boot_ack_emitted = true;
			m_suppress_boot_ack_echo = true;
			trace_p0_write("boot_ack");
		} else {
			trace_p0_write("drop_prelink");
		}
		return;
	}
	if (port == 0U) {
		// SERVICE_TX emits one TX-ring byte per MAIN_LOOP pass. That is far outside the
		// CP-byte IRQ window, so every non-idle P0 write is a CP reply, not a UI event.
		note_cp_tx_byte(value);
		trace_p0_write("cp_tx");
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

void pcd3349a_contract::pulse_cp_byte_irq(std::uint32_t finish_cycles)
{
	auto *core = dynamic_cast<millennium_am8048_core *>(&m_core);
	std::uint8_t const rx_head_before = core ? core->debug_ram(0x20U) : 0U;
	m_core.assert_external_interrupt(true);
	if (core) {
		for (unsigned spins = 0U; spins < 512U && core->debug_ram(0x20U) == rx_head_before; ++spins)
			m_core.step_cycles(1U);
	} else {
		m_core.step_cycles(1U);
	}
	m_core.assert_external_interrupt(false);
	m_core.step_cycles(finish_cycles);
	// Ensure the ROM RX ring drains even if the interrupted PC is not in MAIN_LOOP yet.
	if (auto const *core = dynamic_cast<millennium_am8048_core const *>(&m_core)) {
		for (unsigned spin = 0U; spin < 262144U; ++spin) {
			if (core->debug_ram(0x20U) == core->debug_ram(0x21U))
				break;
			m_core.step_cycles(1U);
		}
	}
}

void pcd3349a_contract::step_tp_cycles_raw(std::uint64_t tp_cycles)
{
	if (tp_cycles > 0U)
		m_core.step_cycles(tp_cycles);
	commit_p0_tx_latch();
}

void pcd3349a_contract::service_hook_timer_tail()
{
	// ROM defers RESP_HOOK_ON/OFF by HOOK_STATE_DELAY_IRQs timer IRQs after the transition byte.
	for (unsigned pass = 0U; pass < 24U; ++pass) {
		m_core.step_cycles(65536U);
		commit_p0_tx_latch();
	}
}

void pcd3349a_contract::note_cp_tx_byte(std::uint8_t value)
{
	m_tx_bytes.push_back(value);
	m_obs.tx_pending = true;
	// Hook steady / keypad bytes are also UI observations. The boot ACK stays on the CP TX path only.
	if ((value >= 0x20U && value < 0x60U) || value == 0x6cU || value == 0x6eU)
		m_ui_events.push_back(value);
}

void pcd3349a_contract::commit_p0_tx_latch()
{
	if (!m_p0_tx_latch_valid)
		return;
	std::uint8_t const first = m_p0_tx_latch;
	m_p0_tx_latch_valid = false;
	note_cp_tx_byte(first);
	while (!m_p0_tx_overrun_bytes.empty()) {
		std::uint8_t const b = m_p0_tx_overrun_bytes.front();
		m_p0_tx_overrun_bytes.pop_front();
		note_cp_tx_byte(b);
	}
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
