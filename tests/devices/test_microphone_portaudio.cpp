// SPDX-License-Identifier: GPL-2.0-or-later
#include <cmath>
#include <cstdlib>
#include <iostream>

void *coinline_mic_open();
void coinline_mic_close(void *);
float coinline_mic_read(void *);

namespace {
bool captured_voice()
{
	void *mic = coinline_mic_open();
	if (!mic)
		return false;
	double power = 0;
	for (unsigned i = 0; i < 600; ++i) {
		float const value = coinline_mic_read(mic);
		power += value * value;
	}
	for (unsigned i = 0; i < 1000; ++i)
		coinline_mic_read(mic);
	bool const silent = coinline_mic_read(mic) == 0.f;
	coinline_mic_close(mic);
	return silent && std::sqrt(power / 600) > 0.15;
}

bool unavailable()
{
	void *mic = coinline_mic_open();
	bool const result = !mic && coinline_mic_read(nullptr) == 0.f;
	coinline_mic_close(mic);
	return result;
}
} // namespace

int main()
{
	unsetenv("COINLINE_MIC_INPUT");
	unsetenv("COINLINE_PA_FIXTURE_NO_INPUT");
	if (!captured_voice()) {
		std::cerr << "expected default PortAudio capture, 44.1 kHz stereo conversion and bounded silence\n";
		return 1;
	}
	setenv("COINLINE_MIC_INPUT", "fixture MICROPHONE B", 1);
	if (!captured_voice())
		return 2;
	setenv("COINLINE_MIC_INPUT", "Fixture microphone", 1);
	if (!unavailable())
		return 3;
	setenv("COINLINE_MIC_INPUT", "Fixture speaker", 1);
	if (!unavailable())
		return 4;
	setenv("COINLINE_MIC_INPUT", "No matching input", 1);
	if (!unavailable())
		return 5;
	unsetenv("COINLINE_MIC_INPUT");
	setenv("COINLINE_PA_FIXTURE_NO_INPUT", "1", 1);
	if (!unavailable())
		return 6;
	unsetenv("COINLINE_PA_FIXTURE_NO_INPUT");
	return captured_voice() ? 0 : 7;
}
