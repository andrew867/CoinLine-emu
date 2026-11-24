# Emulator firmware inputs

The OEM package expects an operator-supplied 256 KiB image at
`firmware/oem/flash.bin`. See [ROM input layout](../BUILDING.md) for the companion
Voiceware image paths.

`tp_pcd3349a_behavioural.rom` is generated from the emulator's original
[telephony controller assembly](../tools/tp8048/) during the release build.
