// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_io.h"

#include "millennium_state.h"

// Port-backed handlers below correlate with `io-trace.jsonl` for audio/call-state proof alongside device traces.

void millennium_configure_io_map(address_map &map, millennium_state &state)
{
	// Board peripherals are low-byte decoded on Z180 I/O cycles.
	// Firmware frequently uses 16-bit port addresses (OUT (C),A style),
	// so mirror handlers across the upper byte to avoid false unmapped I/O.
	map.global_mask(0xff);

	// Later entries override earlier ones for the same port (MAME I/O map merge).
	// Catch-all first logs true external unmapped ports; board handlers below replace it.
	map(0x00, 0xff).rw(state, FUNC(millennium_state::catch_all_io_r), FUNC(millennium_state::catch_all_io_w));

	map(0x40, 0x40).rw(state, FUNC(millennium_state::board_status_r), FUNC(millennium_state::board_status_w));
	// 82C55-class PIO at 0x41–0x44: modem/VFD/voice-bank/coin/relay images — not the front-panel matrix.
	// Handset, keypad, hookswitch, and softkeys are modeled on KEYMATRIX / TERMINAL21_SOFTKEYS for TP→CP CSI/O only.
	map(0x41, 0x44).rw(state, FUNC(millennium_state::pio_keypad_r), FUNC(millennium_state::pio_keypad_w));
	// Voice phrase port — millennium_voiceware_device (VOICEWR / 0x61).
	map(0x61, 0x61).rw(state, FUNC(millennium_state::voiceware_phrase_r), FUNC(millennium_state::voiceware_phrase_w));
	map(0x60, 0x60).rw(state, FUNC(millennium_state::vfd_status_r), FUNC(millennium_state::vfd_display_w));
	map(0x52, 0x52).rw(state, FUNC(millennium_state::card_status_r), FUNC(millennium_state::card_status_w));
	map(0x53, 0x53).rw(state, FUNC(millennium_state::card_data_r), FUNC(millennium_state::card_data_w));
	map(0x54, 0x54).r(state, FUNC(millennium_state::coin_status_r));
	map(0x55, 0x55).w(state, FUNC(millennium_state::coin_control_w));
	// Alerter audio (tone select @ 0x58; DTMF ascii/duration @ 0x59–0x5a; volume @ 0x5b).
	map(0x58, 0x58).w(state, FUNC(millennium_state::audio_tone_w));
	map(0x59, 0x59).w(state, FUNC(millennium_state::audio_dtmf_ascii_w));
	map(0x5a, 0x5a).w(state, FUNC(millennium_state::audio_dtmf_duration_w));
	map(0x5b, 0x5b).w(state, FUNC(millennium_state::audio_vol_w));
	// External 16550-style UART block at 0xE0 (coin/validator path).
	map(0xe0, 0xe7).rw(state, FUNC(millennium_state::external_uart_r), FUNC(millennium_state::external_uart_w));
	// PIO extension @ 0x63 — smart-card socket power/clock enables (board init writes this early).
	map(0x63, 0x63).rw(state, FUNC(millennium_state::pio_port_g_r), FUNC(millennium_state::pio_port_g_w));
	// MACH PIO \c 0xC0–\c 0xC3 — port H latch + cash/status merge; D/E/F card/SAM/EPM glue (\c millennium_mach_async.h bit defs).
	map(0xc0, 0xc3).rw(state, FUNC(millennium_state::mach_pio_r), FUNC(millennium_state::mach_pio_w));
}
