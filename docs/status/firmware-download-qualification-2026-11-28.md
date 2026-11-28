# MTR 2.x firmware download qualification

**28 November 2026.** The stock download application completed a scheduled
firmware update on an isolated two-line standard universal emulator profile.
The test used a registered 1 MiB lab image and a synthetic compatible target
that changes one erased 4 KiB program block. No field terminal or customer
data was involved.

## Baseline

The independently installed cold and warm baseline runs both had active flash
SHA-256 `b09f9c64817f52522cdb4a01f43cdfe5422eb65cd087defeec2906e597d60e34`
and reported program tag `1.3`. Their firmware-written battery hashes were
`e962d7ca989e98465fa4c0a131b370a95fb1261f4b66fb37beccb5edc160b71a`
after cold INSTALL and
`4b5bbf6a13a974040e183b704a5b361bf4bc559391f751335b8a2277d13f8ddf`
after the separate warm-service run.

## Scheduled transfer and apply

CoinLine delivered one MT94 record on the ordinary NCC route. The terminal
acknowledged it with MT14, released that call, then dialed the distinct
loopback code-server route at the assigned time. The answering modem model
completed the guest-controlled answer, mark, S1 and QAM training sequence
before asserting carrier.

The download application requested 4 KiB block 128. The code server returned
four 1 KiB XMODEM packets and one padded 128-byte packet, then EOT. The native
terminal emitted six ACK controls, with no NAK or CAN. Its display progressed
through checksum calculation and block programming, then restarted the main
application and deleted the completed download timers.

The persisted active image changed only block 128 and checksum-metadata block
191. Its SHA-256 is
`38c8b8d802920a5e43fa52f3739bd0d9d85127e7b30b74915ee685ee1e77b142`,
which exactly matches the fixture's independently generated expected active
image. The unchanged target program tag is `1.3`. On another power cycle the
terminal reported `DLA 1 active ver 7` and `X1.3`, retained the same active
flash hash, accepted a physical receiver-lift event, and returned on-hook.

The application restarts its main program in software after apply. This is
observable in the application display and renewed telephony initialization;
it does not cause a second MAME machine-reset milestone in the same process.

## Interrupted transfer and retained retry

The interrupt case closed the code-server connection immediately after
packet 2 was sent. The terminal had ACKed packet 1, then emitted CAN and
released the modem when the connection disappeared. Read-only comparison
showed that active flash was still byte-identical to the baseline. The
terminal displayed `Redial 1` with a retained callback time.

The exact interrupted battery and flash state was then power-cycled. Without
a second MT94 assignment, the download application called the code server
again. Its request reason changed from 1 to 2 while retaining block 128. The
retry completed all five packets and EOT with no NAK or CAN, applied the block,
restarted the main application, and produced the same expected active SHA-256
shown above. A further warm boot preserved that result.

## Automated coverage

- `test_modem_code_server` covers the native answering-modem training state,
  a complete 4 KiB binary bridge, relay release, and the unchanged NCC route.
- `test_firmware_download_staging` and `test_firmware_download_apply` are now
  registered CTest targets under the `firmware_download` label.
- The fixture and read-only evidence tests cover deterministic target
  creation, firmware-maintained checksum metadata, persisted image comparison,
  and mismatch reporting.

The three focused C++ tests passed. The two Python fixture modules passed all
14 tests. Raw wire and emulator captures remain under ignored `build/` run
directories; they are not committed.
