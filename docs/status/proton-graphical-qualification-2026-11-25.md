# NLA1X09 graphical firmware qualification

NLA1X09 is a separate MTR 2.12-based graphical Multi-Pay variant (terminal
type 40, DLA 1.3). Its supplied feature configuration includes Proton, SAM,
ADSI, the 11-line display, the universal terminal role and the local
graphical Proton applet. Graphical advertising, GPM103, IOPR and the data
jack are disabled in this variant. These are firmware configuration claims,
not acceptance results.

The mapped 1 MiB flash SHA-256 is registered in
`fixtures/firmware/firmware-hashes.json`. Automatic hardware selection uses
the later Multi-Pay 1.3 preset and the 11-line expansion. The launcher keeps
its battery state separate from NLA1X08 by the full mapped-flash digest.

## Native cold run

The run in `build/firmware-acceptance/proton-install/20261125T205551.040006Z/`
started from an empty battery directory and used the firmware's normal
cabinet, keypad and handset inputs. The display reached the not-installed
prompt, entered INSTALL, completed CO detection with result 00 and proceeded
to line-number entry. With a synthetic lab identity and the local central
office, it also completed answer detection with result 00, and then displayed
the normal NCC download prompt. The download was held pending a dedicated
table-set assignment. No installed memory image or installation flag was
preloaded, and no NCC download was started.

Full NCC provisioning, INSTALL completion, warm restart, application
recognition, graphical applet behavior, SAM interactions and ADSI are
independent outstanding qualification gates. Generic asynchronous contact
transport is already covered by the native laboratory fixtures; that does
not establish a historical payment application or payment acceptance.
