// SPDX-License-Identifier: GPL-2.0-or-later
// A deterministic PortAudio ABI fixture, not a host recording device.
#include <portaudio.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <numbers>

namespace {
struct fixture_stream {
	PaStreamCallback *callback;
	void *context;
};

PaDeviceInfo const devices[] = {
	{2, "Fixture microphone A", 0, 2, 0, 0.01, 0., 0.04, 0., 44100.},
	{2, "Fixture microphone B", 0, 2, 0, 0.01, 0., 0.04, 0., 44100.},
	{2, "Fixture speaker", 0, 0, 2, 0., 0.01, 0., 0.04, 48000.},
};
} // namespace

extern "C" {
PaError Pa_Initialize() { return paNoError; }
PaError Pa_Terminate() { return paNoError; }
PaDeviceIndex Pa_GetDeviceCount() { return 3; }
PaDeviceIndex Pa_GetDefaultInputDevice() { return std::getenv("COINLINE_PA_FIXTURE_NO_INPUT") ? paNoDevice : 0; }
PaDeviceInfo const *Pa_GetDeviceInfo(PaDeviceIndex device) { return device >= 0 && device < 3 ? &devices[device] : nullptr; }
char const *Pa_GetErrorText(PaError) { return "fixture format unsupported"; }

PaError Pa_IsFormatSupported(PaStreamParameters const *input, PaStreamParameters const *output, double rate)
{
	return input && !output && input->device >= 0 && input->device < 2 && input->channelCount == 2 &&
		input->sampleFormat == paInt16 && rate == 44100. ? paFormatIsSupported : paInvalidSampleRate;
}

PaError Pa_OpenStream(PaStream **stream, PaStreamParameters const *input, PaStreamParameters const *output,
	double rate, unsigned long, PaStreamFlags, PaStreamCallback *callback, void *context)
{
	if (Pa_IsFormatSupported(input, output, rate) != paFormatIsSupported || !callback)
		return paInvalidSampleRate;
	*stream = new fixture_stream{callback, context};
	return paNoError;
}

PaError Pa_StartStream(PaStream *stream)
{
	auto &fixture = *static_cast<fixture_stream *>(stream);
	std::array<std::int16_t, 441 * 2> samples;
	PaStreamCallbackTimeInfo info{};
	for (unsigned block = 0; block < 10; ++block) {
		for (unsigned i = 0; i < 441; ++i) {
			double const tone = std::sin(2. * std::numbers::pi * 700. * (block * 441 + i) / 44100.);
			samples[i * 2] = static_cast<std::int16_t>(12000 * tone);
			samples[i * 2 + 1] = static_cast<std::int16_t>(4000 * tone);
		}
		if (fixture.callback(samples.data(), nullptr, 441, &info, 0, fixture.context) != paContinue)
			return paInternalError;
	}
	return paNoError;
}

PaError Pa_AbortStream(PaStream *) { return paNoError; }
PaError Pa_CloseStream(PaStream *stream)
{
	delete static_cast<fixture_stream *>(stream);
	return paNoError;
}
} // extern "C"
