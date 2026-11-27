// SPDX-License-Identifier: GPL-2.0-or-later
// Default-input capture. Kept out of the MAME device translation unit so the
// multimedia headers are not parsed after the emulator's Windows wrappers.

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
#endif

void *coinline_mic_open()
{
#if defined(_WIN32)
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
#else
	return nullptr;
#endif
}

void coinline_mic_close(void *opaque)
{
#if defined(_WIN32)
	delete static_cast<mic_capture *>(opaque);
#else
	(void)opaque;
#endif
}

float coinline_mic_read(void *opaque)
{
#if defined(_WIN32)
	if (auto *mic = static_cast<mic_capture *>(opaque))
		return mic->pcm.read();
#else
	(void)opaque;
#endif
	return 0.f;
}
