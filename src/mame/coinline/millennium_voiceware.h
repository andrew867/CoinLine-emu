// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "emu.h"

#include "millennium_audio_route.h"
#include "millennium_audio_trace.h"
#include "millennium_voiceware_config.h"
#include "cpu/z180/z180.h"
#include "sound/upd7759.h"
#include "speaker.h"

#include <cstdint>
#include <string>
#include <vector>

DECLARE_DEVICE_TYPE(MILLENNIUM_VOICEWARE, millennium_voiceware_device)

/// Routes firmware voiceware I/O to a board-format uPD7759 ADPCM decoder with real ROM banking.
/// Phrase start/stop drives `millennium_audio_route_device::notify_voice_active` (prompt path for call-state traces).
class millennium_voiceware_device : public device_t, public device_sound_interface {
public:
	millennium_voiceware_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock = 0);

	void device_start() override ATTR_COLD;
	void device_reset() override;
	void sound_stream_update(sound_stream &stream) override;

	void write_hw_control(std::uint8_t data, std::uint16_t pc, std::uint64_t cpu_cycle);
	void write_bank(std::uint8_t data, std::uint16_t pc, std::uint64_t cpu_cycle);
	std::uint8_t read_phrase_port(std::uint16_t pc, std::uint64_t cpu_cycle);
	void write_phrase(std::uint8_t data, std::uint16_t pc, std::uint64_t cpu_cycle);

	/// True while the board-format decoder still has PCM pending.
	bool playing() const noexcept;

private:
	static constexpr std::uint8_t VOICE_RESET_MASK = 0x08U;

	required_device<upd7759_device> m_upd;
	required_device<millennium_audio_route_device> m_audio_route;
	required_device<z180_device> m_z180;
	optional_device<speaker_device> m_vwspk;

	bool m_reset_asserted = false;
	bool m_bank_latched = false;
	std::uint8_t m_bank = 0;
	std::uint8_t m_last_phrase = 0;
	std::uint8_t m_last_chip_bank = 0xffU;
	sound_stream *m_stream = nullptr;
	std::vector<std::int16_t> m_phrase_pcm;
	std::size_t m_phrase_pos = 0;
	bool m_decoder_playing = false;
	/// Latch: was the chip non-idle on the previous phrase-port read (for trace).
	bool m_had_playback = false;
	bool m_upd7759_core_env = false;
	std::string m_voice_rom_sha256_u16;
	std::string m_voice_rom_sha256_u26;
	coinline_voiceware_phrase_port_levels m_phrase_port_levels{};
	coinline_voiceware_retrigger_policy m_retrigger_policy =
		coinline_voiceware_retrigger_policy::suppress_duplicate_strobe_while_playing;

	std::uint64_t emulated_ns() const;
	void emit_row(char const *event_type, std::uint64_t cpu_cycle, std::uint16_t pc, std::uint16_t port, char rw,
		std::uint8_t value, char const *compat);
	void apply_rom_bank_from_latch();
	void phrase_trace_attach_rom_fingerprint(coinline_voiceware_phrase_trace &tr) const;
	/// Software ADPCM → PCM path (legacy). When `m_upd7759_core_env`, used only if
	/// `COINLINE_VOICEWARE_LEGACY_FALLBACK` is enabled after lookup failure.
	bool decode_phrase(std::uint8_t phrase);
};
