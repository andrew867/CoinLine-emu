# Runtime ROM inputs

The portable launcher uses separate firmware inputs for the demo and OEM profiles, plus two Voiceware images. See the [exact paths and sizes](../BUILDING.md#include-your-local-rom-inputs). Supply the supported images that you are authorized to use. For a private local package, pass the staged directory with `--rom-input`.

The default ZIP and the source repository leave these files for the operator to supply. Keep local ROM packages and their contents private. The bundled telephony-processor model image is part of the emulator project and is staged separately by the builder.
