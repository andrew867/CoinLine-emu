# Millennium 11-line VFD and softkey emulator handoff

## Objective

Implement the optional 11-line graphical vacuum fluorescent display and its associated twelve-position button scanner as a hardware profile of the Millennium terminal. The same large-display hardware is used by graphical-terminal and ADSI applications. The emulator must render the actual firmware-driven pixel output and deliver physical button presses through the appropriate hardware interface, without reproducing the terminal's application or menu logic in the GUI.

This is a source-independent interaction contract. It contains observed register semantics, display geometry, synthetic test stimuli and a proposed artwork overlay. No proprietary source identifiers, source excerpts, original screen artwork or ROM contents are required to implement it. Where physical behavior is not confirmed by a hardware capture, the requirement identifies the uncertainty.

## 1. Hardware configuration

| Property | Standard display | Large display |
| --- | --- | --- |
| Display type | Two-line character VFD | 11-line graphical VFD |
| Text geometry | 2 rows, 20 characters per row | 11 logical bands, up to 20 standard text characters per band |
| Interface | Existing character-data interface | Separate expansion-board pixel interface |
| Screen storage | Existing two-line character buffer | Four independently addressable display pages |
| Softkey hardware | Not present | Six display-associated keys, four navigation keys and two spare scan positions |
| Firmware applications | Conventional user interface | Conventional interface, ADSI, graphical screens and compatible applets |

Select this entire hardware personality through the board profile. Do not infer the profile from a string printed on a screen, or route large-display traffic through the original two-line character decoder. Preserve the standard profile and its existing display, keys and artwork unchanged.

## 2. Large-display bus contract

The large display is driven by direct writes of bitmap column data, not simply by the two-line character-data port. The address of each write carries both the location and one page-selector bit. This requires the full 16-bit external I/O address to remain observable; truncating it to eight bits loses the pixel addresses.

| External I/O resource | Access | Observed function |
| --- | --- | --- |
| `0x80` | Write | Expansion-board display-control latch |
| `0x80` | Read | Expansion-board status; bit 0 is display-ready |
| `0x81` | Read/write | Separate expansion-board DSP/control interface; do not alias to the display |
| `0x90` | Read/write | Softkey group-select output and active-low key input |
| `0xF0` | Write | Observed display reset-release access, with value zero |
| `0x1280` through `0x17FF` | Write | Pixel-data window for the even-numbered write pages |
| `0x1A80` through `0x1FFF` | Write | Pixel-data window for the odd-numbered write pages |

The conventional character-display I/O address `0x60` is not the graphical pixel window. Do not consume unrelated communications traffic or make pixel writes alias the expansion-board control port. Preserve other expansion-board status signals, including independent FSK and call-alert detection status, when updating display-ready.

### Display-control latch

| Bit(s) | Mask | Meaning |
| --- | --- | --- |
| 0 | `0x01` | Active-low clear strobe, followed by release back high |
| 1 | `0x02` | Brightness/data-address selection; an observed brightness update lowers this bit, writes a value at the pixel base, then raises it again |
| 2 and 3 | `0x0C` | Select the visible display page, 0 through 3 |
| 4 | `0x10` | Display enable line; set during the observed initialization sequence |
| 5 | `0x20` | Lamp-test control, exact electrical polarity to be confirmed |
| 6 | `0x40` | High bit of the write-page selection |
| 7 | `0x80` | Shared expansion-board function; preserve independently |

Treat each latch write as a complete latched value. A change in visible-page selection does not change which page receives pixel writes. A brightness write must not corrupt the graphic at the base pixel location. Clear affects the selected write page, not all pages; initialization clears all four by selecting each in turn. The initial firmware sequence releases reset and performs these page clears. An emulator must not substitute a fabricated welcome image for that behavior.

### Address and page selection

Let `offset` be in the range `0x000` to `0x57F`, and let `page` be in the range 0 to 3.

- Base pixel I/O address: `0x1280 + offset`.
- If the low bit of the selected write page is 1, add `0x0800` to the address.
- The high bit of the write page is bit 6 of the display-control latch.
- Display-page bits 2 and 3 independently choose the page rendered on the glass.

Store 1,408 independently writable bytes per page (`11 × 128 = 1,408 = 0x580`), for 5,632 total logical bytes across four pages. The clear strobe uses the write-page selection established by the latch and page-selecting pixel access. The observed firmware explicitly writes a base address to establish the low write-page bit before issuing a clear.

The pixel bus is described as write-only. Do not invent a memory readback protocol. Return ready status through the separate expansion status path. Exact display-ready timing, reset state before initialization, brightness transfer function and lamp-test polarity require physical confirmation. Define parameters for bench-calibrated timing rather than imposing arbitrary large busy delays that can stall the processor.

## 3. Pixel model and display geometry

### Logical organization

One logical band occupies 128 addressable column bytes. There are 11 bands. Each byte represents a vertical column of graphic dots; the existing font geometry uses 5×7 dot characters, a 6-column character advance, and a nominal visible line width of 126 columns. The 128-column stride and 126-column visible-width constant must remain distinct. Independent synthetic tests of the historical bitmap conversion utility established that its artwork format packs **eight** source rows into each byte. Existing graphical bitmap payloads also use bit 7, so it must never be discarded solely because the text glyph height is seven dots.

For raster display, implement a candidate active grid of 126 columns by 88 dot positions (`11 × 8`), including all eight bits of each column. Retain the complete 128-column logical stride in memory and never wrap padding columns into an adjacent band. Physical dot pitch, bezel aperture, gap between the bands and the optical placement of the eighth bit have not been independently measured. A 126×88 logical presentation is a candidate interpretation of the verified bitmap packing and address geometry, not a confirmed panel part-number datasheet specification. Keep the optical rendering configurable if a physical display shows a different pitch or gap.

| Coordinate convention | Meaning |
| --- | --- |
| Display column | 0 is left; increase to the right |
| Address band | 0 is the bottom band; 10 is the top band |
| Visible GUI row | 0 is at the top; row = 10 minus address band |
| Offset | `128 × address_band + column` |
| Character placement | A standard character advances six columns; five are used for the glyph and the spacing is part of the written raster |

The standard text coordinate conversion uses an initial six-column margin, while a maintenance text path can begin at column zero. Implement raw pixel placement as authoritative and let the actual firmware select the offsets. Do not enforce a text-only clipping region, font substitution, text reflow or a second arbitrary row reversal. Synthetic utility probes establish low-order bit 0 at the top of each eight-row source band, bit 7 at the bottom, and bottom-band-first output. Validate the equivalent optical orientation using deliberately asymmetric glyphs or a bench capture.

### Rendering behavior

- Render every pixel from the currently displayed page. The main processor's bitmap writes are authoritative, including partial glyphs, icons, advertisements and blank columns.
- Offscreen writes remain invisible until the display page changes. Page changes must be atomic from the user's perspective, with no mixture of two pages.
- Clear, display disable and lamp-test behavior must be distinct from erasing unrelated pages.
- Scrolling, flashing, graphical animation, active-line markers and labels are generated by firmware pixel writes. Do not independently animate or synthesize them in the VFD device. In particular, blinking must follow observed pixel updates from the running terminal, not a second timer in the artwork layer.
- The firmware maintains a five-band working image of 640 bytes (`5 × 128`). This is processor-side state, not a substitute for the four-page peripheral memory. The emulator peripheral should retain actual writes without attempting to duplicate application-side shadow-buffer logic.

### Display content regions

The top two bands are available for conventional call prompts. Graphical presentation uses a larger screen region, including a 112-column by nine-logical-band bitmap arrangement (112×72 source pixels, 1008 packed bytes) and a separate icon/text region. A complete graphical-screen bitmap record additionally has 36 zero filler bytes. The six labelled selection keys occupy paired left/right positions in the lower part of the display. Their associated labels appear at visible rows 6, 8 and 10 (zero-based), corresponding to address bands 4, 2 and 0. Left labels are left-justified and right labels are right-justified. Labels have up to ten characters; an optional active-key indicator can reduce the available label width.

These are content conventions, not hard partitions in peripheral RAM. A full-width graphic can use the entire visible pixel area. Never overlay static softkey captions on emitted pixels or infer active actions from guessed screen text.

## 4. Softkey hardware and input contract

The 11-line firmware directly scans an expansion-board I/O register. Do not assume the existing telephony-processor keyboard-message path alone is sufficient for the large-display profile. A GUI press must affect the emulated hardware key state visible to this direct scanner. The resulting logical key events are also documented below for observing the firmware's higher-level behavior.

The selector occupies output bits 0 through 2 of I/O address `0x90`. A scan selects the three groups using one-hot values `0x01`, `0x02`, and `0x04`, preserving other latched output bits. Input bits 3 through 6 (`0x78`) represent four switch positions within the selected group. They are active low: an unpressed group's masked value is `0x78` and a pressed key clears its associated bit. Other bits are independent or unverified and must not be fabricated by the softkey model.

| Selected group | Input bit 3 (`0x08`) | Input bit 4 (`0x10`) | Input bit 5 (`0x20`) | Input bit 6 (`0x40`) |
| --- | --- | --- | --- | --- |
| `0x01` | Top left, `0x90` | Top right, `0x93` | Up, `0x96` | Right, `0x99` |
| `0x02` | Middle left, `0x91` | Middle right, `0x94` | Down, `0x97` | Spare 1, `0x9A` |
| `0x04` | Bottom left, `0x92` | Bottom right, `0x95` | Left, `0x98` | Spare 2, `0x9B` |

The values `0x90` through `0x9B` in the table are logical key-event identifiers, not I/O addresses for individual keys. The physical input interface is the group-scanned register at `0x90`.

### Input timing and arbitration

- Preserve the physical pressed/released state until the actual input changes. Mouse and keyboard input must be momentary, not toggles.
- The observed terminal scan repeats every 20 ms. A stable single key is accepted on the third matching scan. A continuously held key is not repeatedly emitted; a release or a change to another valid key allows the next press to be qualified.
- More than one switch active across the entire three-group scan is treated as an illegal combination and produces no key action. The physical model should still expose the concurrent switch states so firmware can make that determination.
- Do not add a second mandatory debounce delay to the raw hardware input. The running firmware is responsible for applying its scan/debounce policy. For a purely abstract event-based fixture, the same three-sample semantics must be represented explicitly.
- Deliver each physical press only once through the chosen hardware route. Do not simultaneously inject the same event through the telephony processor and direct expansion-board scanner. Keep ordinary numerical, volume, language, next-call and hookswitch events on their existing paths.
- Spare scan positions must be representable for testing, but need not be visibly labeled in the default artwork. Firmware may ignore these positions. Do not assign them application actions.

## 5. GUI and artwork overlay

### Existing front-panel image

The current front-panel artwork is 643 by 1,515 pixels. It depicts a wide, dark upper display recess, with a much smaller two-line module inserted near its bottom. The existing two-line presentation uses approximately `(219, 360)` to `(558, 444)` in the artwork coordinate system. The large-display version should reuse the same terminal photo and occupy the existing larger recess rather than stretching the small two-line panel or adding a detached second screen.

The following proposed coordinates are measured against the existing photograph and are **GUI design coordinates**, not verified mechanical measurements of an original graphical-terminal bezel. Keep them configurable for future comparison with a physical graphical terminal.

| Overlay element | Left | Top | Right | Bottom |
| --- | ---: | ---: | ---: | ---: |
| Large recess mask / graphical bezel | 126 | 151 | 591 | 462 |
| Active large VFD raster | 177 | 158 | 537 | 409 |
| Top-left momentary key | 143 | 294 | 171 | 318 |
| Middle-left momentary key | 143 | 340 | 171 | 364 |
| Bottom-left momentary key | 143 | 386 | 171 | 410 |
| Top-right momentary key | 543 | 294 | 571 | 318 |
| Middle-right momentary key | 543 | 340 | 571 | 364 |
| Bottom-right momentary key | 543 | 386 | 571 | 410 |
| Navigation left | 279 | 420 | 310 | 447 |
| Navigation up | 316 | 420 | 347 | 447 |
| Navigation down | 353 | 420 | 384 | 447 |
| Navigation right | 390 | 420 | 421 | 447 |

The 360×251 proposed active viewport is close to the candidate 126×88 raster aspect ratio (360/251 and 126/88 are each approximately 1.43). With an 11-band layout, its row-6, row-8 and row-10 centers are approximately y=306, 352 and 398. The six side-key centers align to those label positions. Recalibrate them together if the active viewport changes. The navigation row uses the otherwise unused space below the active display, above the photographed existing volume, language and next-call controls.

### Visual arrangement

```text
        within the existing large dark display recess
   +--------------------------------------------------+
   |           [ two conventional prompt bands ]      |
   |                                                  |
   |           [ firmware-driven graphical area ]     |
   |                                                  |
   | [key]  left label                right label [key]|
   |                                                  |
   | [key]  left label                right label [key]|
   |                                                  |
   | [key]  left label                right label [key]|
   |                 [<] [^] [v] [>]                  |
   +--------------------------------------------------+
        existing small front-panel buttons remain below
```

Use a dark, tinted glass surround with a subtle bezel. Mask the inset two-line module, its unused holder space and fasteners only for the large-display profile. Keep the terminal's original body, handset, keypad, coin and payment controls intact. Draw the six physical side keys as neutral small buttons with a visible but restrained pressed state. The changing captions belong to the display pixels and must not be burned into the physical button art. Do not claim the proposed replacement trim is an exact original hardware reproduction.

Render the VFD dots with the existing terminal's blue-green phosphor aesthetic, dark inactive pixels and restrained glow. Avoid smoothing that converts bitmap text into modern antialiased type. Use nearest-neighbor or an equivalent crisp pixel scaler, preserve aspect ratio and fit within the viewport. Make an optional debug overlay show page number, raw raster coordinates and hitboxes; do not include it in the ordinary hardware view.

### Interaction requirements

1. Select the artwork view from the active hardware profile so the 2-line and 11-line images do not overlap.
2. Make each of the six selection buttons independently clickable and provide remappable keyboard bindings for all six selection and four navigation inputs.
3. Provide clear depressed/released feedback without changing the firmware-driven label text.
4. Preserve the other existing click regions and their coordinate scaling. The added buttons must not cover or remap the original numeric keypad, handset or payment controls.
5. Use the same coordinate transform for the front image, raster and hit targets. Test resizing and headless capture; the hardware state must not depend on displaying the artwork.

## 6. Synthetic conformance tests

Use synthetic data patterns and neutral labels instead of redistributing original images, font tables, terminal ads or ROM data. Every result must be traceable to a real register cycle or a user input state. The following tests are independent of a particular codebase and can be implemented in any emulator.

| ID | Stimulus | Expected output |
| --- | --- | --- |
| V11-01 | Select large-display profile and release reset | Expansion-board display interface becomes active; no hard-coded boot text appears |
| V11-02 | Write a single synthetic column at the base window with normal pixel-write mode | Only the addressed byte in the active write page changes |
| V11-03 | Write at offsets `0x07F`, `0x080`, `0x500`, and `0x57F` | Distinct logical columns/bands; padding does not wrap; top and bottom bands remain distinguishable |
| V11-04 | Use the even/odd address windows with control bit 6 clear, then set | Four independent pages receive four distinct synthetic patterns |
| V11-05 | Write a pattern to an offscreen page, then change the visible page | Image is unchanged until the display-page selection changes |
| V11-06 | Select and clear one page using the observed clear-strobe convention | Only that page becomes blank; the other three retain their bytes |
| V11-07 | Select brightness mode, perform a brightness write at the base, restore pixel-write mode | Brightness state updates; the existing base pixel is unchanged |
| V11-08 | Write asymmetric vertical-column patterns, including bit 7, at selected bands | Raw bit order follows the verified artwork encoding; raster does not mirror left/right or top/bottom; record any remaining uncertainty in the physical optical pitch |
| V11-09 | Write paired left/right label-shaped patterns at address bands 4, 2 and 0 | Labels appear at the proposed side-key row positions; no extra synthesized GUI text |
| V11-10 | Change pixels on a visible page repeatedly | The screen follows the actual writes; no device-side artificial scrolling or blinking |
| V11-11 | No pressed key, read each selected group | Masked read bits equal `0x78` for all groups |
| V11-12 | Independently press each of the twelve scan positions and select the corresponding group | Only the correct input bit is cleared, with the expected logical key identity observed by the terminal |
| V11-13 | Press top-left while selecting groups 1, 2 and 4 | Read mask values are `0x70`, `0x78`, `0x78`, respectively |
| V11-14 | Press two keys in separate groups, then release one | Both raw states are observable; the firmware rejects the multikey scan and accepts the remaining valid single key after qualification |
| V11-15 | Hold a single physical key through several scan periods | Firmware generates one accepted action, not duplicates through multiple input paths |
| V11-16 | Click all ten visible large-display controls in the artwork at two window sizes | Each maps to its matching physical input and releases correctly; no existing hit target is displaced |
| V11-17 | Run identical large-display I/O stimuli headless and with artwork | Backing-page contents and input results are identical |
| V11-18 | Boot a compatible 11-line application and exercise normal prompts, graphical menus and ADSI screens when provisioned | Genuine firmware output updates the pixel display and accepts the correct selection keys |
| V11-19 | Boot the existing two-line profile and exercise ordinary input | Existing character display and controls behave as before |

For V11-18, distinguish basic hardware parity, graphical application operation and ADSI service interoperability as separate milestones. A screenshot of a hand-authored advertising fixture is not evidence of successful firmware boot or application compatibility. If live compatible firmware or provisioning is unavailable, report that milestone as pending rather than simulating a success image.

## 7. Implementation order and completion evidence

1. Establish profile selection and the full-address expansion I/O decode. Keep unrelated board resources intact.
2. Implement page storage, pixel writing, reset release, clear, display selection and shared status handling. Validate V11-01 through V11-08.
3. Implement the direct group-scanned physical key interface with explicit held-key state. Validate V11-11 through V11-15 before connecting the GUI.
4. Attach firmware-driven raster presentation and the profile-specific artwork overlay. Validate V11-09, V11-10, V11-16 and V11-17.
5. Run the compatible firmware and perform V11-18 plus two-line regression V11-19. Record actual firmware identity and runtime conditions.

Produce a machine-readable trace of the full I/O address, direction, value, selected page, control-latch transitions and softkey scan state, along with page snapshots and screenshots. Avoid logging unrelated customer, payment or terminal identification data. Preserve a screenshot of the existing two-line view to demonstrate that its layout remains unchanged. Report unverified physical characteristics separately from passed synthetic and firmware-driven tests.

## 8. Remaining evidence gaps

- Exact original large-display panel model, measured dot pitch and optical pixel spacing.
- Definitive physical placement of bit 7, ready/busy timing, brightness response and lamp-test polarity.
- Physical trim and navigation-button arrangement of an original graphical terminal. The proposed overlay is dimensioned against the existing photograph, not a claim of exact mechanical reproduction.
- Successful execution of a newly built compatible 11-line application and its companion loader.

The historical product family is corroborated by Nortel's publicly released [Millennium terminals product bulletin, Release 02, February 2000](https://payphones.telesfor.org/lib/exe/fetch.php?media=smart%3Amillennium%3Adocu%3Afw_pb_02.pdf). The interaction contract above intentionally stands on its own and does not require access to private firmware sources.
