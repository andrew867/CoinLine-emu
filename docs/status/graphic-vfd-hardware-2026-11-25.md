# 11-line graphical display bring-up

The native driver now supports the four-page graphical expansion selected
by the 11-line board profile, independently of the motherboard revision.
The standard character display and its artwork view remain available.

## Observed results

- Native synthetic Z180 program, run `build/firmware-acceptance/graphic-vfd-probe/20261125T034103.970775Z/`: full 16-bit pixel addresses, all four pages, selective clear, independent display selection, brightness access and all twelve raw physical switch positions passed.
- The native PNG matched the independently packed synthetic image at all 11,088 visible dot positions. Headless and artwork runs produced identical four-page snapshots.
- The new artwork view occupies the existing display recess and exposes six side keys and four navigation keys. The original Standard view markup is unchanged. Standard firmware boot probes both passed in `build/firmware-acceptance/runs/20261125T033226.826047Z/`.
- A separate standard artwork screenshot was captured during genuine firmware startup in `build/firmware-acceptance/graphic-vfd-probe/20261125T033228.202542Z/standard/`.
- Both MTR 1.9 OEM program candidates completed full INSTALL and returned to service after a separate warm launch using the new I/O dispatch. Each acknowledged eleven NCC records; results are in `build/firmware-acceptance/runs/20261125T033629.676687Z/`.
- Native Windows build passed; C++ suite passed 63 tests with two skipped and no failures. Fifteen fixture tests and eleven bitmap-converter tests passed.

## Remaining gates

- No compatible 11-line application image has been qualified. Normal graphical INSTALL, firmware-mediated key qualification, graphical menus and ADSI interoperability remain pending.
- The new artwork input regions use the existing MAME input mechanism. The twelve raw inputs have been exercised through the scanner; actual pointer clicks and resize behavior still require interactive qualification.
- Display-ready timing, lamp-test polarity, brightness transfer function and exact optical geometry are provisional. The independent expansion DSP, FSK and call-alert functions are outside this display model and require their own qualification.
