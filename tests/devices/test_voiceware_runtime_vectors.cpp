// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdint>
#include <iostream>

namespace {

struct voiceware_runtime_model {
	bool reset_asserted = false;
	bool bank_latched = false;
	std::uint8_t bank = 0;
	bool playing = false;
	bool prompt_active = false;
	bool fault_when_reset = false;

	void write_control(std::uint8_t v) { reset_asserted = (v & 0x08U) != 0U; }
	void write_bank(std::uint8_t v)
	{
		bank_latched = true;
		bank = static_cast<std::uint8_t>(v & 0x0FU);
	}
	void write_phrase(std::uint8_t /*phrase*/)
	{
		if (reset_asserted) {
			fault_when_reset = true;
			return;
		}
		playing = true;
		prompt_active = true;
	}
	std::uint8_t read_phrase_port() const { return playing ? 0x7FU : 0xFFU; }
	void on_playback_complete()
	{
		playing = false;
		prompt_active = false;
	}
};

} // namespace

int main()
{
	{
		voiceware_runtime_model m;
		m.write_control(0x00U);
		m.write_bank(0x03U);
		m.write_phrase(0xB3U);
		if (m.read_phrase_port() != 0x7FU || !m.prompt_active || !m.bank_latched || m.bank != 0x03U) {
			std::cerr << "voiceware start/busy vector failed\n";
			return 1;
		}
		m.on_playback_complete();
		if (m.read_phrase_port() != 0xFFU || m.prompt_active) {
			std::cerr << "voiceware completion/ready vector failed\n";
			return 2;
		}
	}

	{
		voiceware_runtime_model m;
		m.write_control(0x08U);
		m.write_phrase(0x10U);
		if (!m.fault_when_reset || m.prompt_active || m.read_phrase_port() != 0xFFU) {
			std::cerr << "voiceware reset gate vector failed\n";
			return 3;
		}
	}

	std::cout << "voiceware_runtime_vectors ok\n";
	return 0;
}

