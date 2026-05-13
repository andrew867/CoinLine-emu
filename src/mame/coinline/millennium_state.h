// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "emu.h"
#include "cpu/z180/z180.h"
#include "rendfont.h"
#include "screen.h"

#include <memory>

#include "millennium_audio.h"
#include "millennium_board_profile.h"
#include "millennium_card.h"
#include "millennium_coin.h"
#include "millennium_hostbridge.h"
#include "millennium_keypad.h"
#include "millennium_modem.h"
#include "millennium_nvram.h"
#include "millennium_microwire_93c66.h"
#include "millennium_security.h"
#include "millennium_smartcard.h"
#include "millennium_sam.h"
#include "millennium_voiceware.h"
#include "millennium_audio_route.h"
#include "millennium_supervision.h"
#include "millennium_telephony.h"
#include "millennium_vfd.h"
#include "millennium_rtos_startup_model.h"
#include "millennium_terminal_peripherals_model.h"
#include "millennium_z180_boot_init_model.h"
#include "millennium_pcd3349a.h"

#include "sound/beep.h"
#include "sound/flt_rc.h"
#include "sound/upd7759.h"
#include "millennium_z180_snapshot.h"

#include <array>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <unordered_map>
#include <vector>

class millennium_state : public driver_device {
public:
	millennium_state(machine_config const &mconfig, device_type type, char const *tag);

	void millennium(machine_config &config);
	void memory_map(address_map &map);
	void io_map(address_map &map);

	uint32_t screen_update(screen_device &screen, bitmap_rgb32 &bitmap, rectangle const &cliprect);

	u8 vfd_status_r();
	void vfd_display_w(u8 data);

	u8 catch_all_io_r(offs_t offset);
	void catch_all_io_w(offs_t offset, u8 data);
	u8 voiceware_phrase_r();
	void voiceware_phrase_w(u8 data);

	u8 board_status_r();
	void board_status_w(u8 data);
	u8 pio_keypad_r(offs_t offset);
	void pio_keypad_w(offs_t offset, u8 data);

	virtual void driver_start() override ATTR_COLD;
	virtual void machine_start() override ATTR_COLD;
	virtual void machine_reset() override;
	void device_stop() override ATTR_COLD;

	/// MAME 0.287 uses scheduler timers with \c s32 param (no \c device_timer_id API).
	void schedule_driver_timer(s32 id, attotime const &delay);
	TIMER_CALLBACK_MEMBER(driver_timer_cb);

	virtual tiny_rom_entry const *device_rom_region() const override ATTR_COLD;
	virtual ioport_constructor device_input_ports() const override ATTR_COLD;

	u8 phys_ram_r(offs_t offset);
	void phys_ram_w(offs_t offset, u8 data);

	/// Physical 0x00000–0xBFFFF: flash image + overlay for writes (MMU may map logical stack here).
	u8 phys_low_r(offs_t offset);
	void phys_low_w(offs_t offset, u8 data);

	millennium_memory_layout_config const &memory_layout() const noexcept { return m_memory_layout; }

	u8 storage_nvram_r(offs_t offset);
	void storage_nvram_w(offs_t offset, u8 data);
	u8 storage_table_r(offs_t offset);
	void storage_table_w(offs_t offset, u8 data);
	u8 storage_dla_r(offs_t offset);
	void storage_dla_w(offs_t offset, u8 data);

	u8 card_status_r();
	void card_status_w(u8 data);
	u8 card_data_r();
	void card_data_w(u8 data);
	u8 mach_pio_r(offs_t offset);
	void mach_pio_w(offs_t offset, u8 data);
	u8 external_uart_r(offs_t offset);
	void external_uart_w(offs_t offset, u8 data);

	u8 pio_port_g_r();
	void pio_port_g_w(u8 data);

	u8 coin_status_r();
	void coin_control_w(u8 data);
	void audio_tone_w(u8 data);
	void audio_dtmf_ascii_w(u8 data);
	void audio_dtmf_duration_w(u8 data);
	void audio_vol_w(u8 data);

	// IP communications (control <-> telephony processor) over Z180 CSIO + board status bits.

private:
	void ipcomm_queue_rx_byte(u8 data, bool priority = false);
	void ipcomm_complete_csio_tx_byte(std::uint8_t byte);
	static constexpr s32 TID_HOST_POLL = 2;
	static constexpr s32 TID_CARD_UI = 3;
	static constexpr s32 TID_COIN_UI = 4;
	static constexpr s32 TID_M3_FALLBACK = 5;
	static constexpr s32 TID_CPU_TRACE = 6;
	/// High-rate opcode / PC band probe for vector / interrupt / context-save diagnosis (not a cycle-accurate hook).
	static constexpr s32 TID_VECTOR_PROBE = 7;
	/// uPD7759 end-of-segment: firmware routes speech completion through Z180 INT0.
	static constexpr s32 TID_VOICE_INT0 = 8;
	static constexpr std::uint64_t CPU_TRACE_LINEAR_MAX = 50000ULL;
	static constexpr std::size_t CPU_TRACE_RING_MAX = 10000U;
	static constexpr std::size_t COMPACT_RING_MAX = 4096U;
	static constexpr unsigned VECTOR_EVENT_BUDGET_MAX = 500000U;
public:
	enum class trace_profile : std::uint8_t { fast, m6, uart, voice, full, tp_timing };
	enum class tp_backend_kind : std::uint8_t { legacy, pcd3349a };
	/// `COINLINE_TEL_RESPONSE_POLICY`: how strictly runtime C4 / health frames follow fault-display ordering.
	enum class tel_response_policy : std::uint8_t {
		immediate,
		latch_then_clear,
		withhold_until_not_responding_seen,
		withhold_until_retry,
		withhold_until_timeout,
	};
private:

	required_device<z180_device> m_maincpu;
	required_device<screen_device> m_screen;
	required_device<millennium_vfd_device> m_vfd;
	required_device<millennium_keypad_device> m_keypad;
	required_device<millennium_security_device> m_security;
	required_device<millennium_modem_device> m_modem;
	required_device<millennium_hostbridge_device> m_hostbridge;
	required_device<millennium_nvram_device> m_nvram;
	required_device<millennium_card_device> m_card;
	required_device<millennium_smartcard_device> m_smartcard;
	required_device<millennium_sam_device> m_sam;
	required_device<millennium_coin_device> m_coin;
	required_device<millennium_audio_device> m_audio;
	required_device<millennium_voiceware_device> m_voiceware;
	required_device<upd7759_device> m_voicew_upd;
	required_device<filter_rc_device> m_vw_upd_filter;
	required_device<beep_device> m_earpiece_tone_a;
	required_device<beep_device> m_earpiece_tone_b;
	required_device<millennium_audio_route_device> m_audio_route;
	required_device<millennium_supervision_device> m_supervision;
	required_device<millennium_telephony_device> m_telephony;

	required_ioport m_cardui;
	required_ioport m_coinui;
	required_ioport m_keymatrix_io;
	required_ioport m_secmask_io;
	required_ioport m_linectrl_io;
	required_ioport m_terminal21_softkeys_io;

	std::filesystem::path m_boot_trace_path;
	std::filesystem::path m_io_trace_path;
	std::filesystem::path m_unknown_port_path;
	std::filesystem::path m_memory_trace_path;
	std::filesystem::path m_nvram_storage_trace_path;
	std::filesystem::path m_microwire_trace_path;
	std::filesystem::path m_cpu_trace_path;
	std::filesystem::path m_z180_reg_trace_path;
	std::filesystem::path m_stack_trace_path;
	std::filesystem::path m_ram_init_trace_path;
	std::filesystem::path m_mmu_translation_trace_path;
	std::filesystem::path m_interrupt_trace_path;
	std::filesystem::path m_timer_trace_path;
	std::filesystem::path m_asci_trace_path;
	std::filesystem::path m_reset_trace_path;
	std::filesystem::path m_interrupt_events_path;
	std::filesystem::path m_vector_events_path;
	std::filesystem::path m_context_switch_events_path;
	std::filesystem::path m_eidi_events_path;
	std::filesystem::path m_voiceware_trace_path;
	std::filesystem::path m_external_uart_trace_path;
	std::filesystem::path m_telephony_board_trace_path;
	std::filesystem::path m_telephony_handshake_trace_path;
	std::filesystem::path m_telephony_phase_trace_path;
	std::filesystem::path m_telephony_ready_decision_trace_path;
	std::filesystem::path m_telephony_rx_buffer_trace_path;
	std::filesystem::path m_telephony_parser_trace_path;
	std::filesystem::path m_service_path_trace_path;
	std::filesystem::path m_service_task_trace_path;
	std::filesystem::path m_service_display_trace_path;
	std::filesystem::path m_display_queue_trace_path;
	std::filesystem::path m_service_refresh_trace_path;
	std::filesystem::path m_oos_message_selector_trace_path;
	std::filesystem::path m_oos_reason_trace_path;
	std::filesystem::path m_service_mode_trace_path;
	std::filesystem::path m_display_cache_trace_path;
	std::filesystem::path m_telephony_ready_state_trace_path;
	std::filesystem::path m_telephony_retry_timeout_trace_path;
	std::filesystem::path m_termflag_trace_path;
	std::filesystem::path m_alarm_condition_trace_path;
	std::filesystem::path m_rtos_signal_trace_path;
	std::filesystem::path m_service_timer_trace_path;
	std::filesystem::path m_telephony_runtime_conversation_trace_path;
	std::filesystem::path m_telephony_runtime_health_trace_path;
	std::filesystem::path m_telephony_runtime_state_path;
	std::filesystem::path m_tp_csio_raw_trace_path;
	std::filesystem::path m_tp_csio_qualified_trace_path;
	std::filesystem::path m_tp_command_response_trace_path;
	std::filesystem::path m_tp_runtime_health_trace_path;
	std::filesystem::path m_tp_readiness_sequence_trace_path;
	std::filesystem::path m_tp_board_status_trace_path;
	std::filesystem::path m_hw_cntl_relay_trace_path;
	std::filesystem::path m_vfd_message_state_trace_path;
	std::filesystem::path m_front_panel_trace_path;
	std::filesystem::path m_front_panel_input_source_trace_path;
	std::filesystem::path m_fetch_provenance_trace_path;
	std::filesystem::path m_stack_control_flow_trace_path;
	std::filesystem::path m_first_pc_ffff_path;
	std::filesystem::path m_first_pc_ffff_context_path;
	std::filesystem::path m_first_rst38_context_path;
	std::filesystem::path m_vfd_trace_path;
	std::filesystem::path m_vfd_snapshots_path;
	std::filesystem::path m_vfd_final_state_path;
	std::filesystem::path m_vfd_final_text_path;
	std::filesystem::path m_vfd_idle_fixture_diff_trace_path;
	std::filesystem::path m_tp_keypad_input_trace_path;
	std::filesystem::path m_tp_keypad_event_trace_path;
	std::filesystem::path m_tp_cp_keypad_protocol_trace_path;
	std::filesystem::path m_craft_entry_gate_trace_path;
	std::filesystem::path m_tp_health_cadence_trace_path;
	std::filesystem::path m_tp_csio_timing_trace_path;
	std::filesystem::path m_tp_interrupt_trace_path;
	std::filesystem::path m_tp_timeout_trace_path;
	std::filesystem::path m_tp_8048_runtime_trace_path;
	std::filesystem::path m_tp_8048_port_trace_path;
	std::filesystem::path m_tp_8048_keypad_trace_path;
	std::filesystem::path m_tp_8048_tone_trace_path;
	std::filesystem::path m_tp_8048_cp_protocol_trace_path;
	std::unordered_map<std::string, std::uint64_t> m_tp_cadence_prev_cycle_by_event;
	std::uint64_t m_tp_last_csio_timing_cycle = 0ULL;
	std::uint64_t m_tp_last_host_poll_cycle = 0ULL;
	unsigned m_stack_trace_remaining = 0U;
	unsigned m_ram_init_trace_remaining = 0U;
	std::string m_firmware_sha256_hex;
	std::string m_board_profile_json;
	millennium_z180_board_config m_z180_board{};
	millennium_display_profile m_display_profile{};
	millennium_keypad_board_config m_keypad_board{};
	millennium_security_board_config m_security_board{};
	millennium_coin_board_config m_coin_board{};
	millennium_alerter_board_config m_audio_board{};
	millennium_memory_layout_config m_memory_layout{};
	millennium_user_io_board_config m_user_io_board{};
	/// Optional terminal_09 data-jack relay stepping when `overlay.data_jack_manual_keypad_active` (see `tp_enqueue_ui_event`).
	coinline::terminal::data_jack_model m_data_jack_model{};
	bool m_m5_logged = false;
	bool m_m5v_logged = false;
	bool m_m5a_logged = false;
	bool m_m5c_logged = false;
	/// True on last poll when the board-format Voiceware decoder had pending PCM.
	bool m_voicew_chip_was_active = false;
	/// true == no pending Voiceware PCM on last `TID_VOICE_INT0` poll.
	bool m_voicew_prev_upd_idle = true;
	/// Voice-completion INT0 is a hardware level from the speech path; firmware clears it by starting/resetting speech.
	bool m_voicew_int0_asserted = false;
	bool m_last_modem_dcd = false;
	bool m_m6_logged = false;
	/// Boot-trace milestone M7 (PIO column-strobe depth heuristic without CP seeing KEYMATRIX); separate from M7A/M7B/M7C.
	bool m_m7_logged = false;
	bool m_m8_logged = false;
	bool m_m9_logged = false;
	bool m_m10_logged = false;
	bool m_boot_protocol_ready_milestone_logged = false;
	bool m_boot_acceptance_ready_milestone_logged = false;
	unsigned m_vfd_idle_fixture_diff_counter = 0U;
	std::string m_cp_install_key_buffer_model;
	bool m_craft_gate_accept_traced = false;
	bool m_craft_code_detected_traced = false;
	bool m_craft_screen_vfd_trace_logged = false;
	bool m_asci_baseline_ready = false;
	std::uint8_t m_base_cntla0 = 0;
	std::uint8_t m_base_cntlb0 = 0;
	std::uint8_t m_unknown_default = 0xff;
	std::array<bool, 256> m_io_known_or_suspected{};

	std::vector<u8> m_phys_ram;
	millennium_microwire_93c66 m_microwire_93c66;
	/// Writable overlay for physical addresses below 0xC0000 (invalid entries read from flash).
	std::vector<u8> m_phys_low_overlay;
	std::vector<u8> m_phys_low_valid;
	std::uint64_t m_ram_write_events = 0;
	bool m_m3_m4_logged = false;

	void sync_microwire_from_nvram();
	void load_firmware_and_emit_m0();
	void load_io_fixture_masks();
	void emit_trace_m1_m2();
	void emit_boot_m3_m4_snapshot(char const *m3_trigger);
	std::string resolve_relative_path(std::string const &rel) const;

	char const *current_boot_milestone_tag() const;
	void emit_io_trace_line(char const *tag, std::uint16_t port, char rw, std::uint8_t data);
	void log_unknown_external_port(std::uint16_t port_full, bool is_write, std::uint8_t value);
	void poll_voice_segment_done_pulse_int0();
	void clear_voice_segment_int0(char const *reason);
	void append_memory_trace_line(std::uint32_t phys_addr, std::uint8_t data);
	void init_auxiliary_trace_sinks();
	void maybe_trace_memory_sidecar(std::uint32_t phys_addr, std::uint8_t data, char const *region_tag);
	void append_mmu_translation_trace_line();
	void append_supplemental_cpu_trace_samples();
	void sync_modem_asci_lines();
	void cpu_trace_sample_tick();
	void append_cpu_trace_line(std::string const &line);
	void flush_cpu_trace_ring_to_disk();
	void vector_event_probe_tick();
	std::uint8_t read_physical_debug_byte(std::uint32_t phys_addr) const;
	char const *physical_debug_source(std::uint32_t phys_addr) const;
	std::string format_fetch_provenance_event(char const *event, std::uint16_t pc, std::uint16_t sp,
		std::uint8_t op0, std::uint8_t op1, std::uint8_t op2);
	void remember_memory_write(std::uint32_t phys_addr, std::uint8_t data, char const *region_tag);
	void maybe_emit_stack_control_flow(std::uint64_t cyc, std::uint16_t pc, std::uint16_t sp, std::uint8_t op0,
		std::uint8_t op1, std::uint8_t op2, char const *milestone, bool new_decode_slot);
	millennium_z180_snapshot build_z180_snapshot();
	void write_stop_debug_artifacts();

	void ensure_vfd_render_font();

	void try_boot_m5_keypad_only(u64 cycle);
	void try_boot_m7_from_keypad(u64 cycle);
	void try_boot_m8(u64 cycle);

	void poll_card_ui();
	void poll_coin_ui();

	void telephony_note_csio_rx_byte(std::uint8_t b);
	void telephony_maybe_log_m7_rx_path(char const *reason);
	std::string vfd_row_text(int row) const;
	void telephony_runtime_trace_event(char const *event, char const *command, char const *response, bool checksum_ok,
		char const *note);
	void try_emit_boot_readiness_milestones(std::uint64_t cycle);
	void trace_tp_keypad_input_edge(std::uint32_t mask, std::uint8_t tp_code, char const *reason);
	void trace_tp_keypad_event(char const *stage, std::uint8_t tp_code, char const *reason);
	void trace_tp_cp_keypad_delivered(std::uint64_t cycle, std::uint16_t pc, std::uint8_t byte);
	void trace_craft_gate_line(std::string const &json_fields);
	void maybe_emit_vfd_idle_fixture_diff(std::uint64_t cycle);
	void note_craft_entry_key(u8 code, std::uint64_t cycle);
	void refresh_craft_entry_window(std::uint64_t cycle);
	void set_tel_hook_state(bool on_hook, std::uint64_t cycle, char const *reason);
	void prime_ipcomm_rx_shift_from_queues();
	unsigned ipcomm_pending_rx_count() const noexcept;
	bool qualify_cp_to_tp_csio_byte(std::uint64_t cycle, std::uint16_t pc, std::uint16_t sp, std::uint8_t cntr_before,
		std::uint8_t cntr_after, std::uint8_t byte, char const *edge_source, std::string &reject_reason);
	void trace_cp_to_tp_raw_candidate(std::uint64_t cycle, std::uint16_t pc, std::uint16_t sp, std::uint8_t cntr_before,
		std::uint8_t cntr_after, std::uint8_t trdr, std::uint8_t byte, char const *edge_source, char const *candidate_source,
		bool accepted, char const *reject_reason);
	void trace_cp_to_tp_qualified(std::uint64_t cycle, std::uint16_t pc, std::uint16_t sp, std::uint8_t byte,
		char const *parser_before, char const *parser_after, bool checksum_ok);
	void trace_tp_command_accepted(std::uint8_t byte, bool response_queued, char const *response_label, char const *reason);
	void trace_tp_response_emitted(std::initializer_list<std::uint8_t> bytes, char const *label, bool checksum_ok);
	void tp_enqueue_ui_event(std::uint8_t code, char const *reason);
	bool user_io_cp_policy_absorb_tp_opcode(std::uint8_t code, char const **rule_id_out,
		char const **expected_consumer_out) const;
	void user_io_harness_policy_absorb(std::uint8_t code, char const *tp_reason, char const *rule_id,
		char const *expected_consumer);
	void tp_process_front_panel_events(std::uint32_t keymatrix, std::uint32_t linectrl, std::uint32_t softkeys_raw);
	bool tel_runtime_may_emit_c4() const noexcept;
	void tel_queue_runtime_keepalive_c4_c0(u64 cy);
	void append_user_io_harness_trace(char const *event, char const *reason);
	void write_user_io_harness_summary();
	void write_user_io_harness_failure_report_if_needed();
	void update_earpiece_tone_output(std::uint64_t cycle, std::uint16_t pc);
	void tp_backend_process_front_panel(std::uint32_t keymatrix, std::uint32_t linectrl, std::uint32_t softkeys_raw,
		std::uint8_t secmask, std::uint64_t cycle, std::uint16_t pc);
	bool tp_backend_handle_cp_byte(std::uint8_t byte);

	u32 m_card_ui_last = 0;
	u32 m_coin_ui_last = 0;
	std::uint8_t m_audio_dtmf_ascii = 0;

	std::array<std::uint8_t, 4> m_mach_pio_shadow{};
	std::array<std::uint8_t, 4> m_pio_8255_shadow{};
	std::array<std::uint8_t, 8> m_ext_uart_shadow{};
	/// Divisor latch bytes (DLAB=1); persist across THR/RBR and IER access when DLAB=0 (16550-style).
	std::uint8_t m_ext_uart_dll = 0x80U;
	std::uint8_t m_ext_uart_dlm = 0x02U;
	std::deque<std::uint8_t> m_ext_uart_rx_queue;
	bool m_ext_uart_boot_seeded = false;
	bool m_ext_uart_tel_up_seen = false;
	bool m_tel_ip_link_enabled = false;
	bool m_tel_boot_code_sent = false;
	std::uint64_t m_tel_reset_cycle_start = 0ULL;
	std::uint64_t m_tel_link_enable_cycle = 0ULL;
	std::uint64_t m_tel_boot_code_deadline_cycle = 0ULL;
	/// When nonzero, CSI/O TP link arms after this cycle (post `PWR_FAIL_LINE` release + firmware delay).
	std::uint64_t m_tel_ip_link_enable_deadline_cycle = 0ULL;
	unsigned m_ext_uart_tx_count = 0U;

	// External UART telephony bring-up (M7A/M7B): source-backed minimal response sequencing.
	enum class tel_uart_phase : std::uint8_t {
		reset = 0,
		wait_rx_enable,
		ack_queued,
		status_queued,
		done,
	};
	tel_uart_phase m_tel_uart_phase = tel_uart_phase::reset;
	bool m_m7a_logged = false;
	bool m_m7_rx_path_logged = false;
	bool m_m7c_logged = false;
	std::uint64_t m_m7c_gate_diag_last_cycle = 0;
	bool m_alarm_tel_not_responding_latched = false;
	bool m_alarm_tel_not_responding_cleared = false;
	bool m_termfg_telephony_up_inferred = false;
	bool m_not_responding_display_seen = false;
	bool m_runtime_oos_seen = false;
	bool m_runtime_not_responding_seen = false;
	u64 m_tel_policy_start_cycle = 0;
	// External UART bring-up trace only (may not drive terminal flags if firmware uses CSIO path).
	bool m_tel_status_frame_ok = false;
	bool m_csio_status_frame_ok = false;
	bool m_csio_error_report_ok = false;
	bool m_csio_ack_seeded = false;
	enum class tel_boot_semantic_state : std::uint8_t {
		reset_pending = 0,
		acked_not_ready,
		status_ready,
		runtime_active,
		timeout_latched,
	};
	tel_boot_semantic_state m_tel_boot_semantic_state = tel_boot_semantic_state::reset_pending;
	bool m_tel_fw_boot_contract_satisfied = false;
	bool m_tel_boot_power_ack_seen = false;
	bool m_tel_boot_hook_state_seen = false;
	bool m_tel_boot_power_status_seen = false;
	bool m_tel_boot_error_report_seen = false;
	bool m_tel_boot_status_seen = false;
	bool m_tel_pending_status_after_clear = false;
	bool m_tel_ready_sequence_completed = false;
	bool m_tel_version_c2_sent = false;
	unsigned m_tel_ready_heartbeat_div = 0U;
	unsigned m_tel_not_responding_poll_count = 0U;
	std::uint64_t m_tp_last_heartbeat_cycle = 0ULL;
	std::uint64_t m_tp_heartbeat_count = 0ULL;
	std::deque<std::uint8_t> m_tel_uart_tx_recent;
	bool m_tel_uart_tx_in_frame = false;
	std::uint8_t m_tel_uart_tx_code = 0;
	std::uint8_t m_tel_uart_tx_len = 0;
	std::uint8_t m_tel_uart_tx_sum = 0;
	unsigned m_tel_uart_tx_remaining = 0;
	bool m_tel_uart_stx_pending = false;
	// RX frame tracker for var-length telephony frames: [code][len][payload...(len-3)][checksum]
	bool m_tel_rx_in_frame = false;
	std::uint8_t m_tel_rx_code = 0;
	std::uint8_t m_tel_rx_len = 0;
	std::uint8_t m_tel_rx_sum = 0;
	unsigned m_tel_rx_remaining = 0;
	bool m_front_panel_trace_seeded = false;
	u32 m_last_keymatrix_state = 0U;
	u32 m_last_secmask_state = 0U;
	u32 m_last_linectrl_state = 0U;
	u32 m_last_firmware_keymatrix_state = 0U;
	u32 m_last_firmware_linectrl_state = 0U;
	u32 m_last_tp_ui_keymatrix_state = 0U;
	u32 m_last_tp_ui_linectrl_state = 0U;
	std::uint32_t m_last_tp_ui_softkeys_state = 0U;
	/// terminal_22_user_io_timing: autorepeat for one held dial/rep key (500 ms then every 150 ms).
	std::uint32_t m_tp_ui_repeat_hold_mask = 0U;
	std::uint64_t m_tp_ui_repeat_hold_start_cy = 0ULL;
	unsigned m_tp_ui_repeat_extra_sent = 0U;
	/// terminal_24_user_io_fault_injection: dial-around release pairing, synthetic release, hook debounce.
	unsigned m_tp_ui_pending_dial_release = 0U;
	std::uint64_t m_tp_ui_dial_release_deadline_cy = 0ULL;
	std::uint32_t m_tp_ui_suppress_dial_until_physical_release = 0U;
	std::uint64_t m_tp_ui_last_hook_transition_cy = 0ULL;
	std::uint64_t m_tp_ui_fault_duplicate_release_count = 0ULL;
	std::uint64_t m_tp_ui_fault_synthetic_release_count = 0ULL;
	std::uint64_t m_tp_ui_fault_hook_integrity_violation_count = 0ULL;
	std::uint64_t m_tp_ui_fault_unknown_opcode_count = 0ULL;
	std::uint64_t m_tp_ui_fault_dropped_event_count = 0ULL;
	std::uint64_t m_tp_ui_fault_illegal_multi_softkey_sample_count = 0ULL;
	std::uint64_t m_tp_ui_fault_abuse_guard_escalation_count = 0ULL;
	/// terminal_22 softkey_scan_contract: debounce before treating 11-line inputs as stable.
	std::uint32_t m_tp_ui_sk_last_raw = 0U;
	std::uint32_t m_tp_ui_sk_stable = 0U;
	std::uint64_t m_tp_ui_sk_stable_deadline_cy = 0ULL;
	/// terminal_21 datajack_abuse_guard: rapid accepted hook transitions (hook-flash-like).
	std::uint64_t m_tp_ui_last_accepted_hook_transition_cy = 0ULL;
	unsigned m_tp_ui_abuse_rapid_hook_transition_accum = 0U;
	bool m_tp_ui_softkey_illegal_episode = false;
	std::filesystem::path m_user_io_harness_trace_path;
	std::filesystem::path m_user_io_harness_summary_path;
	std::filesystem::path m_user_io_harness_failures_path;
	std::string m_user_io_harness_run_id = "unlabeled-run";
	std::vector<std::string> m_user_io_harness_enabled_vectors{"evidence_schema_minimum_fields"};
	/// KEYMATRIX bits for current tp_process_front_panel_events pass (physical hook for CP policy).
	std::uint32_t m_tp_ui_policy_km_snapshot = 0U;
	unsigned m_front_panel_active_read_trace_budget = 0U;
	u8 m_voice_pb_prev = 0xffU;
	u8 m_voice_pb_last_phrase = 0xffU;
	u64 m_voice_pb_last_cycle = 0ULL;

	// Telephony IP-comm emulation (bit-level CSIO)
	bool m_ipcomm_rts_asserted = false; // active low on PIO port A bit 6
	bool m_ipcomm_last_sclk = true;
	std::deque<std::uint8_t> m_ipcomm_rx_prio_bytes;
	std::deque<std::uint8_t> m_ipcomm_rx_bytes;
	std::uint8_t m_ipcomm_rx_shift = 0;
	int m_ipcomm_rx_bit = 0;
	bool m_ipcomm_have_rx_byte = false;
	/// Host-side CSI/O TX framing: after a 0xC0-class header, consume length and trailing octets until checksum so
	/// payload bytes are not mistaken for unrelated single-byte host opcodes.
	bool m_ip_tx_need_length_byte = false;
	unsigned m_ip_tx_skip_remain = 0;
	std::uint8_t m_ip_tx_var_hdr = 0;
	std::uint8_t m_ip_tx_declared_len = 0;
	std::uint64_t m_csio_tx_last_candidate_cycle = 0ULL;
	std::uint64_t m_csio_tx_last_accepted_cycle = 0ULL;
	std::uint8_t m_csio_tx_last_candidate_byte = 0U;
	std::uint8_t m_csio_tx_last_accepted_byte = 0U;
	unsigned m_csio_tx_shift_edge_count = 0U;
	unsigned m_csio_tx_shift_epoch = 0U;
	unsigned m_csio_tx_last_accepted_epoch = 0U;
	// Runtime telephony behavior knobs/state (non-boot periodic health contracts).
	bool m_tel_hook_onhook = true;
	bool m_tel_hook_onhook_stable = true;
	std::uint64_t m_tel_hook_last_change_cycle = 0ULL;
	std::uint64_t m_tel_hook_stabilize_until_cycle = 0ULL;
	bool m_tel_handset_ok = true;
	unsigned m_tel_health_consecutive_miss = 0U;
	unsigned m_tel_stuck_key_counter = 0U;
	unsigned m_tel_handset_bad_counter = 0U;
	bool m_alarm_stuck_keys = false;
	bool m_alarm_handset_discont = false;
	unsigned m_tel_suppress_c4_sweeps_remaining = 0U;
	unsigned m_tel_force_handset_bad_sweeps_remaining = 0U;
	unsigned m_tel_force_key_active_sweeps_remaining = 0U;
	std::uint64_t m_tel_last_health_sweep_cycle = 0ULL;
	std::uint64_t m_tel_last_good_health_cycle = 0ULL;
	std::uint64_t m_tel_fault_last_latched_cycle = 0ULL;
	unsigned m_tel_timeout_relatch_guard_hits = 0U;
	std::uint64_t m_tp_last_ui_event_cycle = 0ULL;
	std::uint64_t m_tel_last_runtime_keepalive_cycle = 0ULL;
	tel_response_policy m_tel_response_policy = tel_response_policy::immediate;
	unsigned m_tel_runtime_poll_count = 0U;
	unsigned m_tel_runtime_timeout_count = 0U;
	unsigned m_tel_runtime_retry_count = 0U;
	unsigned m_tel_runtime_reset_signal_count = 0U;
	std::string m_tel_last_runtime_poll_command;
	std::string m_tel_last_runtime_response;
	bool m_tel_runtime_waiting_c4 = false;
	std::uint64_t m_tel_runtime_wait_c4_deadline_cycle = 0ULL;
	bool m_tel_init_dialogue_window_active = false;
	std::uint64_t m_tel_init_dialogue_window_start_cycle = 0ULL;
	unsigned m_tel_init_dialogue_step = 0U;
	// Model of telephony -> control bytes as presented on the CSI/O link (firmware ISR feeds ip_rx_buffer).
	bool m_csio_rx_in_frame = false;
	std::uint8_t m_csio_rx_code = 0;
	std::uint8_t m_csio_rx_len = 0;
	std::uint8_t m_csio_rx_sum = 0;
	unsigned m_csio_rx_remaining = 0;
	bool m_vfd_is_oos = false;
	bool m_vfd_is_not_responding = false;
	bool m_craft_entry_window_active = false;
	bool m_craft_entry_sequence_complete = false;
	std::string m_craft_entry_progress;
	std::uint64_t m_craft_entry_window_start_cycle = 0ULL;
	std::uint64_t m_craft_entry_window_last_input_cycle = 0ULL;
	std::uint64_t m_craft_entry_sequence_complete_cycle = 0ULL;
	enum class earpiece_tone_mode : std::uint8_t { none, dialtone, nis };
	earpiece_tone_mode m_earpiece_tone_mode = earpiece_tone_mode::none;
	trace_profile m_trace_profile = trace_profile::fast;
	bool m_trace_capture_full_io = false;
	bool m_trace_capture_full_cpu = false;
	bool m_trace_capture_fault_context = true;
	bool m_trace_capture_hot_summary = false;
	unsigned m_uart_io_log_budget = 32U;
	std::array<unsigned, 8> m_uart_read_counts{};
	std::array<unsigned, 8> m_uart_write_counts{};
	std::deque<std::string> m_io_trace_ring;
	std::unordered_map<std::uint16_t, unsigned> m_unknown_io_counts;
	/// HW control port (0x40) output latch (SCLK, voice reset, etc.); read merges low nibble with security state.
	std::uint8_t m_hw_cntl_port_image = 0x80U;
	unsigned m_vector_event_budget = 0U;
	u16 m_vecprobe_last_pc = 0;
	std::uint8_t m_vecprobe_last_op0 = 0xffU;
	bool m_vecprobe_last_iff1 = false;
	bool m_vecprobe_last_iff2 = false;
	bool m_vecprobe_in_ctx_band = false;
	bool m_first_pc_ffff_seen = false;
	bool m_first_rst38_seen = false;
	unsigned m_first_pc_ffff_next_remaining = 0U;
	unsigned m_first_rst38_next_remaining = 0U;
	std::deque<std::string> m_pc_fault_context_ring;
	std::string m_last_io_event_json;
	std::string m_last_stack_write_json;
	std::unordered_map<std::uint32_t, std::string> m_last_memory_write_by_phys;
	/// PIO port G (0x63): smart-card socket power/clock enables and related status bits.
	std::uint8_t m_pio_port_g = 0xff;

	/// Telephony routing trace: HW_CNTL R1–R3 and PIO-C data-jack / MUTDTMF bits (same JSONL as HW_CNTL relay trace).
	void trace_hw_cntl_telephony_relays(u8 prev, u8 data, std::uint64_t cy, u16 pc, u16 sp);
	void trace_pio_port_c_telephony_routing(u8 prev, u8 data, std::uint64_t cy, u16 pc, u16 sp);

	void maybe_io_trace(char const *tag, std::uint16_t port, char rw, std::uint8_t data);
	bool profile_should_log_io(char const *tag, std::uint16_t port, char rw);
	void append_io_trace_line_profiled(std::string const &line);
	void flush_io_trace_ring_to_disk(char const *reason);
	void write_hot_summary_files();

	std::uint64_t m_cpu_trace_total_lines = 0;
	bool m_cpu_trace_ring_mode = false;
	std::deque<std::string> m_cpu_trace_ring;

	std::uint16_t m_last_trace_pc = 0;
	std::uint16_t m_last_trace_sp = 0;
	std::uint16_t m_last_trace_port = 0;

	/// Raw JSON of the idle VFD reference (`display.idle_fixture` in board profile) for M10 detection.
	std::string m_idle_display_fixture_json;

	std::unique_ptr<render_font> m_panel_font;
	/// VFD: "Millennium" face from `artwork/Millenft.ttf` (Win: AddFontResourceEx) or gfxfont fallback.
	std::unique_ptr<render_font> m_vfd_font;
	bool m_vfd_render_prepared = false;
	bool m_vfd_use_gfxfont_fallback = true;
	coinline::z180::boot_init_model m_boot_init_contract{};
	coinline::rtos::startup_scheduler_model m_rtos_startup_contract{};
	tp_backend_kind m_tp_backend_kind = tp_backend_kind::legacy;
	std::unique_ptr<millennium_pcd3349a> m_tp_pcd3349a;
#ifdef _WIN32
	bool m_win_vfd_font_added = false;
	std::wstring m_win_vfd_font_path;
#endif
};
