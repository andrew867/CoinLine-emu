# MTR 2.x firmware download qualification correction

**28 November 2026.** The previously recorded end-to-end result is retracted.
The run executed stock terminal firmware in the emulator, but its host side was
a purpose-built NCC and code-server fixture. It did not use the production
CoinLine scheduler and hosted code-server path. The transferred image was a
generated marker target, not a separate registered firmware release, and the
terminal continued to report the baseline version.

The preserved evidence report also recorded
`service_and_restart_qualified: false`. An exact computed post-apply hash did
not override that result. The earlier status text incorrectly treated the
fixture result as production qualification.

## What the fixture run established

- Stock terminal firmware accepted a fixture-delivered MT94 and emitted MT14.
- The terminal dialed a distinct loopback code-server route and completed the
  emulated modem training states.
- The fixture transferred one generated 4 KiB block by the documented packet
  framing. The terminal changed persistent flash and retained retry state after
  an interrupted fixture transfer.

These are component observations only. They are not evidence that production
CoinLine can host the inbound DLA session or deliver a registered firmware
release.

## Gates still open

- Production CoinLine must schedule and deliver MT94 over its normal NCC path.
- Its hosted code server must accept the terminal's inbound DLA call, validate
  the request and send selected blocks from approved registered storage.
- A distinct registered target must be transferred without generated marker
  content.
- Guest-issued NOR erase and program operations must be captured from the live
  run, and the resulting active image must exactly match the registered target.
- A separate restart must boot that target, production CoinLine must observe
  its target identity and version, and receiver service must remain functional.
- Interrupted-transfer recovery must pass on the same production path.

The evidence command now fails unless all of those observations are supplied
and true. The generated marker-image helper has been removed.
