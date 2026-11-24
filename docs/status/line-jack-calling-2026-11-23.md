# Line jack calling verification

The local central office supplies dial tone and detects the digits outpulsed by the terminal. On incoming calls it asserts ring for two seconds and clears it for four seconds. The handset can answer during either part of the cadence.

The Windows SIP package received an incoming SignalWire call and acknowledged the SIP dialog. Audio from Windows capture reached the calling side through the handset and SIP media path. A far-end BYE interrupted the terminal's loop current. The off-hook terminal returned to its dialing prompt.

The Windows handset captures continuous input at 48 kHz and filters it to the line jack's 8 kHz PCM rate. The emulated transmit gate controls the microphone path. See [running the emulator](../../RUNNING.md) for SIP and capture-device settings.
