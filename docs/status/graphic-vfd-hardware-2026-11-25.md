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
- The NLA1X08 MTR 2.12-based graphical/ADSI image, terminal type 40 with internal DLA version 1.3, booted and wrote its own out-of-service and Craft prompts. The initial trace in `build/firmware-acceptance/graphic-firmware/20261125T041119.921760Z/` established that normal display traffic uses bit 5 high and brightness code 3. The corrected profile uses active-low lamp test and four provisional brightness levels.
- From blank terminal storage, NLA1X08 completed the CO and answer detection tests with status 00, acknowledged all 18 NCC records, displayed download status 02, passed its keypad and magnetic-card checks, and completed calibration and escrow with status 00 after a firmware-offered calibration retry. The chip-card test reported failure. After closing and locking the cabinet and removing the card, the original firmware displayed **Please lift receiver**.
- The cold-run evidence is under `build/firmware-acceptance/graphic-firmware/20261125T042328.999166Z/`. The native display captured every one of the 16 downloaded MT131/132 bitmap screens in 92 snapshots. Each exact 112 by 72 pixel match was placed at column 8, row 16 on the 126 by 88 pixel VFD. MT133 was acknowledged; menu application operation was not established by this bitmap comparison.
- A separate artwork launch loaded the cold run's saved battery and program flash. After an initial telephony alarm and a normal NCC alarm upload, the firmware returned to **Please lift receiver** without Craft intervention. Lifting the handset displayed the normal deposit, card or dial prompt, and returning it restored idle. The warm evidence is in `build/firmware-acceptance/graphic-firmware/20261125T045651.482325Z/`. Both emulator processes exited successfully.

## Remaining gates

- The original cold-run smart-card self-test failed. A subsequent pin-level model passed Craft item 728 twice on the installed NLA1X08, and passed a complete standard NLA1X05 INSTALL and restart. See [card contact qualification](smartcard-contact-2026-11-25.md). A fresh NLA1X08 INSTALL and funded-card transactions still require separate evidence. The startup telephony alarm recovered during normal maintenance in this run; its cause is not qualified. Graphical menu navigation and ADSI service interoperability require their own evidence.
- The new artwork input regions use the existing MAME input mechanism. The twelve raw inputs have been exercised through the scanner; actual pointer clicks and resize behavior still require interactive qualification.
- Display-ready timing, brightness transfer function and exact optical geometry still require physical calibration. The independent expansion DSP, FSK and call-alert functions are outside this display model and require their own qualification.
