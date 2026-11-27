// SPDX-License-Identifier: GPL-2.0-or-later
// Host microphone capture, independent of the emulated handset/audio clock.
// Keep host multimedia headers out of the MAME device translation unit.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#if defined(_WIN32)
#include "millennium_mic_pcm.h"

#include <atomic>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <thread>
#include <windows.h>
#include <mmsystem.h>

namespace {

std::optional<UINT> capture_device()
{
	wchar_t requested[256];
	DWORD const length = ::GetEnvironmentVariableW(L"COINLINE_MIC_INPUT", requested, std::size(requested));
	if (length == 0)
		return WAVE_MAPPER;
	if (length >= std::size(requested)) {
		std::fprintf(stderr, "microphone input name is too long\n");
		return std::nullopt;
	}
	std::optional<UINT> match;
	UINT const count = ::waveInGetNumDevs();
	for (UINT device = 0; device < count; ++device) {
		WAVEINCAPSW caps{};
		if (::waveInGetDevCapsW(device, &caps, sizeof(caps)) != MMSYSERR_NOERROR ||
				length > std::char_traits<wchar_t>::length(caps.szPname) ||
				::CompareStringOrdinal(caps.szPname, int(length), requested, int(length), TRUE) != CSTR_EQUAL)
			continue;
		if (match) {
			std::fprintf(stderr, "microphone input matches multiple devices\n");
			return std::nullopt;
		}
		match = device;
	}
	if (!match)
		std::fprintf(stderr, "microphone input matches no device\n");
	return match;
}

struct mic_capture {
	millennium_mic_pcm pcm;
	std::atomic<bool> stopping{false};
	std::thread worker;
	HANDLE ready = nullptr;
	HWAVEIN handle = nullptr;
	// Eight 10 ms capture buffers let the WinMM driver keep recording through
	// short host scheduling stalls. The separate PCM ring still bounds delay.
	std::array<WAVEHDR, 8> headers{};
	std::array<std::array<std::int16_t, 480>, 8> buffers{};

	~mic_capture()
	{
		if (worker.joinable()) {
			stopping.store(true, std::memory_order_release);
			::SetEvent(ready);
			worker.join();
		}
		if (handle != nullptr) {
			::waveInReset(handle);
			for (auto &header : headers)
				if (header.dwFlags & WHDR_PREPARED)
					::waveInUnprepareHeader(handle, &header, sizeof(header));
			::waveInClose(handle);
		}
		if (ready != nullptr)
			::CloseHandle(ready);
	}

	void capture()
	{
		std::size_t next = 0;
		while (::WaitForSingleObject(ready, INFINITE) == WAIT_OBJECT_0) {
			// Multiple completions can share one event signal. Read in queue order.
			while (!stopping.load(std::memory_order_acquire)) {
				auto &header = headers[next];
				if (!(header.dwFlags & WHDR_DONE))
					break;
				auto const count = header.dwBytesRecorded / sizeof(std::int16_t);
				pcm.push(buffers[next].data(), std::min(count, buffers[next].size()));
				MMRESULT const queued = ::waveInAddBuffer(handle, &header, sizeof(header));
				if (queued != MMSYSERR_NOERROR) {
					std::fprintf(stderr, "microphone buffer queue failed (%u)\n", unsigned(queued));
					return;
				}
				next = (next + 1) % headers.size();
			}
			if (stopping.load(std::memory_order_acquire))
				return;
		}
		std::fprintf(stderr, "microphone capture event failed\n");
	}
};

} // namespace
#elif defined(__linux__) || defined(__APPLE__)
#include "millennium_mic_input.h"

#if __has_include(<portaudio.h>)
#include <portaudio.h>
#else
// The pinned MAME tree also carries the public PortAudio API header.
#include <portaudio/include/portaudio.h>
#endif

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <dlfcn.h>

namespace {

// Load the platform's PortAudio runtime only when microphone capture is
// requested by the audio device. Local playback and NCC remain available
// if no recording backend or hardware is installed.
struct portaudio_api {
	void *library = nullptr;
	bool initialized = false;
	decltype(&Pa_Initialize) initialize = nullptr;
	decltype(&Pa_Terminate) terminate = nullptr;
	decltype(&Pa_GetDefaultInputDevice) default_input = nullptr;
	decltype(&Pa_GetDeviceCount) device_count = nullptr;
	decltype(&Pa_GetDeviceInfo) device_info = nullptr;
	decltype(&Pa_IsFormatSupported) format_supported = nullptr;
	decltype(&Pa_OpenStream) open_stream = nullptr;
	decltype(&Pa_StartStream) start_stream = nullptr;
	decltype(&Pa_AbortStream) abort_stream = nullptr;
	decltype(&Pa_CloseStream) close_stream = nullptr;
	decltype(&Pa_GetErrorText) error_text = nullptr;

	~portaudio_api()
	{
		if (initialized)
			terminate();
		if (library)
			::dlclose(library);
	}

	template <typename Function> bool load(Function &fn, char const *name)
	{
		fn = reinterpret_cast<Function>(::dlsym(library, name));
		return fn != nullptr;
	}

	bool open()
	{
#if defined(__APPLE__)
		char const *const names[] = { "libportaudio.2.dylib", "/opt/homebrew/lib/libportaudio.2.dylib", "/usr/local/lib/libportaudio.2.dylib" };
#else
		char const *const names[] = { "libportaudio.so.2" };
#endif
		for (auto const *name : names)
			if ((library = ::dlopen(name, RTLD_NOW | RTLD_LOCAL)) != nullptr)
				break;
		if (!library) {
			std::fprintf(stderr, "microphone unavailable: PortAudio runtime not installed\n");
			return false;
		}
		if (!(load(initialize, "Pa_Initialize") && load(terminate, "Pa_Terminate") &&
				load(default_input, "Pa_GetDefaultInputDevice") && load(device_count, "Pa_GetDeviceCount") &&
				load(device_info, "Pa_GetDeviceInfo") && load(format_supported, "Pa_IsFormatSupported") &&
				load(open_stream, "Pa_OpenStream") && load(start_stream, "Pa_StartStream") &&
				load(abort_stream, "Pa_AbortStream") && load(close_stream, "Pa_CloseStream") &&
				load(error_text, "Pa_GetErrorText"))) {
			std::fprintf(stderr, "microphone unavailable: incompatible PortAudio runtime\n");
			return false;
		}
		PaError const result = initialize();
		if (result != paNoError) {
			std::fprintf(stderr, "microphone PortAudio initialization failed: %s\n", error_text(result));
			return false;
		}
		initialized = true;
		return true;
	}

	PaDeviceIndex select_device() const
	{
		char const *requested = std::getenv("COINLINE_MIC_INPUT");
		if (!requested || !*requested)
			return default_input();
		PaDeviceIndex match = paNoDevice;
		PaDeviceIndex const count = device_count();
		for (PaDeviceIndex device = 0; device < count; ++device) {
			auto const *info = device_info(device);
			if (!info || info->maxInputChannels < 1 || !info->name || !millennium_mic_name_matches(info->name, requested))
				continue;
			if (match != paNoDevice) {
				std::fprintf(stderr, "microphone input matches multiple devices\n");
				return paNoDevice;
			}
			match = device;
		}
		return match;
	}
};

struct mic_capture {
	portaudio_api api;
	millennium_mic_pcm pcm;
	millennium_mic_input input;
	PaStream *stream = nullptr;

	~mic_capture()
	{
		if (stream) {
			// Abort and close before destroying either queue or the library.
			api.abort_stream(stream);
			api.close_stream(stream);
		}
	}

	static int capture(void const *samples, void *, unsigned long frames,
		PaStreamCallbackTimeInfo const *, PaStreamCallbackFlags, void *context) noexcept
	{
		auto &mic = *static_cast<mic_capture *>(context);
		mic.input.push(mic.pcm, static_cast<std::int16_t const *>(samples), frames);
		return paContinue;
	}

	bool start()
	{
		if (!api.open())
			return false;
		PaDeviceIndex const device = api.select_device();
		auto const *info = device < 0 ? nullptr : api.device_info(device);
		if (!info || info->maxInputChannels < 1) {
			std::fprintf(stderr, "microphone unavailable: no matching PortAudio input\n");
			return false;
		}

		// Prefer the original 48 kHz mono path. If it is not supported, use
		// a common/native rate at or below 48 kHz, with stereo downmix if needed.
		unsigned const native_rate = std::isfinite(info->defaultSampleRate) && info->defaultSampleRate >= 8000 && info->defaultSampleRate <= 48000
			? static_cast<unsigned>(std::lround(info->defaultSampleRate)) : 48000U;
		std::array<unsigned, 7> const rates{48000, native_rate, 44100, 32000, 24000, 16000, 8000};
		PaError last_error = paInvalidSampleRate;
		for (std::size_t i = 0; i < rates.size(); ++i) {
			unsigned const rate = rates[i];
			if (std::find(rates.begin(), rates.begin() + i, rate) != rates.begin() + i)
				continue;
			for (int channels = 1; channels <= std::min(2, info->maxInputChannels); ++channels) {
				PaStreamParameters params{};
				params.device = device;
				params.channelCount = channels;
				params.sampleFormat = paInt16;
				params.suggestedLatency = std::max(0.02, info->defaultLowInputLatency);
				last_error = api.format_supported(&params, nullptr, rate);
				if (last_error != paFormatIsSupported)
					continue;
				pcm.reset();
				input.configure(rate, channels);
				last_error = api.open_stream(&stream, &params, nullptr, rate,
					paFramesPerBufferUnspecified, paNoFlag, &capture, this);
				if (last_error != paNoError) {
					stream = nullptr;
					continue;
				}
				last_error = api.start_stream(stream);
				if (last_error == paNoError) {
					std::fprintf(stderr, "microphone open PortAudio input (%u Hz, %d channel%s)\n", rate, channels, channels == 1 ? "" : "s");
					return true;
				}
				api.close_stream(stream);
				stream = nullptr;
			}
		}
		std::fprintf(stderr, "microphone PortAudio open failed: %s\n", api.error_text(last_error));
		return false;
	}
};

} // namespace
#endif

void *coinline_mic_open()
{
#if defined(_WIN32)
	wchar_t disabled[2]{};
	if (::GetEnvironmentVariableW(L"COINLINE_MIC_DISABLED", disabled, std::size(disabled)) == 1 && disabled[0] == L'1')
		return nullptr;
	auto const device = capture_device();
	if (!device)
		return nullptr;
	auto mic = std::make_unique<mic_capture>();
	mic->ready = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
	if (mic->ready == nullptr) {
		std::fprintf(stderr, "microphone event creation failed\n");
		return nullptr;
	}
	WAVEFORMATEX fmt{};
	fmt.wFormatTag = WAVE_FORMAT_PCM;
	fmt.nChannels = 1;
	fmt.nSamplesPerSec = 48000;
	fmt.wBitsPerSample = 16;
	fmt.nBlockAlign = 2;
	fmt.nAvgBytesPerSec = 96000;
	MMRESULT const opened = ::waveInOpen(&mic->handle, *device, &fmt,
		reinterpret_cast<DWORD_PTR>(mic->ready), 0, CALLBACK_EVENT);
	if (opened != MMSYSERR_NOERROR || mic->handle == nullptr) {
		std::fprintf(stderr, "microphone open failed (%u)\n", unsigned(opened));
		return nullptr;
	}
	for (std::size_t i = 0; i < mic->headers.size(); ++i) {
		auto &header = mic->headers[i];
		header.lpData = reinterpret_cast<LPSTR>(mic->buffers[i].data());
		header.dwBufferLength = DWORD(sizeof(mic->buffers[i]));
		MMRESULT result = ::waveInPrepareHeader(mic->handle, &header, sizeof(header));
		if (result == MMSYSERR_NOERROR)
			result = ::waveInAddBuffer(mic->handle, &header, sizeof(header));
		if (result != MMSYSERR_NOERROR) {
			std::fprintf(stderr, "microphone buffer setup failed (%u)\n", unsigned(result));
			return nullptr;
		}
	}
	if (::waveInStart(mic->handle) != MMSYSERR_NOERROR) {
		std::fprintf(stderr, "microphone start failed\n");
		return nullptr;
	}
	try {
		mic->worker = std::thread(&mic_capture::capture, mic.get());
	} catch (std::system_error const &) {
		std::fprintf(stderr, "microphone capture thread failed\n");
		return nullptr;
	}
	std::fprintf(stderr, "microphone open %s input\n", *device == WAVE_MAPPER ? "default" : "configured");
	return mic.release();
#elif defined(__linux__) || defined(__APPLE__)
	char const *disabled = std::getenv("COINLINE_MIC_DISABLED");
	if (disabled && std::string_view(disabled) == "1")
		return nullptr;
	auto mic = std::make_unique<mic_capture>();
	return mic->start() ? mic.release() : nullptr;
#else
	return nullptr;
#endif
}

void coinline_mic_close(void *opaque)
{
#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
	delete static_cast<mic_capture *>(opaque);
#else
	(void)opaque;
#endif
}

float coinline_mic_read(void *opaque)
{
#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
	if (auto *mic = static_cast<mic_capture *>(opaque))
		return mic->pcm.read();
#else
	(void)opaque;
#endif
	return 0.f;
}
