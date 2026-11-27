# Regional tariff qualification

**27 November 2026.** CoinLine regional tariff package (#140) and explicit
per-terminal host plan selection were exercised with independent synthetic
lab terminals, normal downloaded tables and physical emulator inputs. No
external telephone or emergency service was called.

## Earlier two-line profile

The independent native matrix passed 15 of 15 controls on the earlier MTR
1.9 reference. The terminal displayed 25 cents for a ten-digit local call,
accepted one synthetic quarter, outpulsed the expected local destination,
and the simulated CO answered. It displayed the expected first-period prices
of 50 cents for national home-area, 100 cents for other Canadian areas and
150 cents for US areas. Directory assistance and relay initial rates were
100 and 25 cents. Blocked shortened and nonlocal ten-digit forms emitted no
outgoing voice digits. Free service, toll-free and information-service
controls preserved their configured dialed forms.

Earlier independent duration controls separately passed a fixed 130-second
local call, a home-area national call with an additional payment period,
and a 90-second simulated relay call. These are emulator/host lab results,
not physical equipment qualification.

## Later graphical profile

The initial 18-case matrix passed 12 controls. Ordinary regional payment
classification was incorrect despite acknowledged and retained price
records. A corrected candidate with SHA-256 prefix `453dc7b58c12` reached
warm service and restored the local 25-cent and national home-area 50-cent
payment prompts, yielding 14 of 18 controls with the original regional
table set. Exact national diagnostic rules displayed the expected US and
Canadian initial rates, separating proper rate handling from missing broad
screening coverage.

A generalized host-rated national configuration was installed through a
normal selected Craft download of two records, both acknowledged. The
terminal displayed 150 and 100 cents for independent US and Canadian
controls, after an ordinary host rate request. The host verified initial and
subsequent 60-second amounts of 150/50 and 100/25 cents respectively. An
inactive-area control returned unavailable status and did not outpulse.
Local, home-area national, nonlocal ten-digit denial, four free service
codes, both toll-free forms and the short and long information-service
aliases retained their observed behavior.

The shortened seven-digit control displayed a misleading local price after
timeout. A separate offline physical quarter trial proved there was no
outpulse, no cashbox increment and the entire quarter was refunded. Correct
early rejection of the unsupported dial form remains a user-interface gate.
Relay and health-information service-code qualification is separate.

A separate ordinary selected download of the service numbering record
completed, with its one application acknowledgment. The relay and health
controls still rejected; community information, directory assistance,
local and home-area national controls retained their previous observed
behavior. The service candidate is therefore not qualified for deployment.

## Coin calibration

The earlier three-window validator and the downloaded later nine-window
coin table use different physical channel layouts. The launcher now makes
the physical calibration an explicit independent choice. With `regional-nine`,
the native graphical terminal requested 25 cents, accepted one synthetic
quarter, outpulsed the local destination and received local CO answer. The
emulated cashbox increased by exactly 25 cents, escrow returned to zero and
the refund total remained unchanged. This test did not change tariff data,
SRAM or EEPROM to force the result.

Both the broad national and the service-code table candidates remain lab
configurations until their relevant acceptance gates are complete.
