# OEM answer supervision and independent launcher profiles

## Regression correction

The OEM INSTALL answer-detection stage reported status 51. The CO log
showed the complete ten-digit manager outpulse followed by modem relay
release, with the handset loop still seized. A previous CO change cleared
the pending manager number on that relay edge, before its scheduled local
answer-polarity sequence. That prevented the expected CO supervision.

The CO now retains a pending exact manager outpulse after modem relay
release. Actual handset or loop release cancels it. A new outpulse after
the released relay begins a separate call; an already answered manager
call also releases normally. The local manager address remains excluded
from external SIP routing. No firmware or saved memory changes are needed.

An actual cold OEM run using the packaged executable and fresh memory,
with a loopback SIP registrar configured, displayed Completed 00 at both
the CO and answer checks. The CO recorded one local answer and no outbound
SIP INVITE. The terminal then requested the NCC download; the probe
stopped without starting a download, and exited normally. Evidence is in
`build/firmware-acceptance/oem-answer/20261125T171755.728035Z/`.

## Dual terminal launchers

The portable package provides independent OEM and graphical launchers.
Each selects its registered firmware and board configuration, uses a
separate session lock, and preserves independent saved state and log paths.
Each CO receives its own ephemeral TCP line jack. SIP configuration lives
in a separate INI file, with distinct default UDP signaling and dynamic
media ranges. A local package may explicitly include a registered graphical
image alongside the OEM image and shared Voiceware pair; public CI packages
contain neither firmware image nor Voiceware.

Both packaged launchers passed native input validation and ran concurrently
for 26 emulated seconds with separate local CO connections. An independent
dual SIP run used a single loopback registrar and two different test accounts.
Both registered on their separate default UDP ports, with both processes
still active and separate line jacks present. Both exited with status 0.
Evidence: `build/firmware-acceptance/dual-profile/probe-ak9nr1mf/`.
External account-to-account calling remains untested.

A later private Windows package combined the standard two-line OEM image
with the independently cold/warm-qualified full graphical image identified
by SHA-256 prefix `0974cea06aaa`. The ZIP integrity and both packaged image
hashes were verified, with no recordings or saved terminal state included.
Both extracted profiles registered concurrently on their separate default
ports at the local test registrar, established separate CO line jacks, and
exited normally. Evidence: `build/firmware-acceptance/dual-profile/probe-3o6q29d5/`.

The subsequent local package contains the independently qualified full
graphical image `996a8ac5e81d` and selects its generic asynchronous contact
fixture by registered digest. The standard OEM default remains synchronous,
and the optional internal module positions remain empty unless configured.
The final ZIP passed integrity and image-hash checks and contained neither
recordings nor saved terminal state. Both packaged profiles again registered
concurrently with distinct ports and separate local CO line jacks and
exited normally. Evidence:
`build/firmware-acceptance/dual-profile/probe-1uubu1ow/`.

All 49 Python fixture and CO tests passed, including the early manager
relay-release cases and isolated profile defaults.
