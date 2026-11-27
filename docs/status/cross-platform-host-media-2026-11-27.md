# Cross-platform SIP and host microphone capture

## Implementation

- Removed the portable launcher's Windows-only SIP restriction. The CO's
  existing SIP signaling, RTP, SRTP and connected DTMF remain unchanged across
  platforms. The selected Python interpreter is checked before the emulator
  starts whenever a SIP account or an exact local ADSI route is enabled.
- Windows retains its existing WinMM 48 kHz capture. Linux and macOS load the
  system PortAudio runtime on demand. Missing library, unavailable recording
  device or unsupported format leaves the microphone silent, without disabling
  local CO, NCC or terminal-generated audio.
- PortAudio prefers 48 kHz mono, with standard/native rates at or below 48 kHz
  and stereo downmix as fallbacks. A bounded, allocation-free callback converts
  the capture to mono 48 kHz and feeds the existing ring and voice-band FIR.
  The same 8 kHz PCM handset circuit and firmware-controlled hybrid gates are
  used on all platforms. The host speaker path remains MAME's native output.
- POSIX archives include an explicit `setup-sip.sh` and pinned Python
  requirements. The script installs a package-local virtual environment;
  both profile launchers use it when present. Local CO operation without
  enabled SIP/ADSI routes has no Python SIP dependency.
- The launcher now applies explicit window resolution on POSIX, validates
  SIP registrar ports, and bounds shutdown of the CO sidecar. Explicit local
  service-only routing configurations can be packaged on any supported host.
- The macOS executable embeds a microphone purpose description for the
  operating system's permission prompt. The `[audio] capture_enabled` opt-out
  disables host recording without changing the telephone audio circuit.

The PortAudio callback uses the public [PortAudio v19 API](https://portaudio.com/docs/v19-doxydocs/api_overview.html).
The Python compatibility packages provide platform-specific wheels for
[audioop-lts](https://pypi.org/project/audioop-lts/0.2.2/) and
[pylibsrtp](https://pypi.org/project/pylibsrtp/1.0.0/).
The macOS capture permission metadata follows [Apple's microphone privacy key](https://developer.apple.com/documentation/bundleresources/information-property-list/nsmicrophoneusagedescription).

## Verification gates

- Windows fixture suite: 51 passed, including simulated POSIX launch preflight
  with and without SIP dependencies and direct ADSI routing.
- Native PCM test under Windows: 48 kHz voice and alias suppression, 44.1 kHz
  and 32 kHz stereo conversion across variable callback blocks, null-input
  silence, bounded backlog and recording-device name matching passed.
- Windows WinMM capture translation unit and the complete native MAME
  subtarget compiled and linked successfully with the current MINGW64
  toolchain.
- Windows embedded Python runtime: 31 CO tests passed, including genuine
  loopback SIP dialogs, connected DTMF and bidirectional RTP.
- Linux and macOS CI independently passed the 31-test SIP loopback suite and a native
  PortAudio ABI fixture. The fixture presents a 44.1 kHz stereo capture
  endpoint, exercises default and explicit selection, rejects ambiguous or
  output-only devices, and tests close/reopen and silence when unavailable.
  Both platforms passed the PCM and PortAudio tests. The native release
  matrix separately compiles the full MAME executable and checks the macOS
  embedded microphone permission key.

Physical microphone access and external-provider calls on Linux/macOS are
independent operator acceptance gates; a synthetic capture fixture is not
evidence that a specific host has a usable recording source. No native
firmware or NCC storage behavior is changed by this host-media work.
