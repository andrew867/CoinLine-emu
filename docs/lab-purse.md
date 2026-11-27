# Independent simulated-value purse

The local `tools/lab_purse.py` library and command-line utility implement
an invented nonredeemable `LAB` unit ledger. This is a separate simulator
application, not a historical card or SAM program. It is not connected to
the native APDU handler, CoinLine payment authorization or firmware
association decisions. The existing native read-only contact fixtures and
three independent module positions remain separate transport tests.
The bundled `smartcard-async-module-lab.json` fixture returns an invented
immutable `MOD0` test object through the generic T=1 transport and may be
selected with `graphic_session.py --sam-slot0`. It contains no purse state
or historical module identifier, and is independent of the customer fixture.

The store uses one SQLite transaction for a validated snapshot, ledger and
incident update. It preserves pending and uncertain reservations, commits
each simulated spend once, records reloads, rejects conflicting IDs and
does not recreate a fixture on a corrupt snapshot. A distinct immutable
local simulator journal is required to reconcile an uncertain event. It
does not obtain an outcome from contact I/O or a native terminal display.
All amounts are integers; the display label is `LAB`, with decimal
formatting only for presentation.

Example local request file for a new test fixture:

```json
{"operation":"seed","fixture_id":"lab-example","initial_units":100,"limit_units":200}
```

Run the request against an isolated local SQLite store:

```sh
python tools/lab_purse.py --store <local-scratch>/purses.sqlite --request <request.json>
```

In a portable package use `app/lab_purse.py` instead of `tools/lab_purse.py`
with its supplied or system Python runtime.

Other allowed model requests are `inspect`, `history`, `begin_spend`,
`complete_spend`, `cancel_spend`, `mark_uncertain`, `reload_fixture`,
`record_local_outcome` and `reconcile_local`. They operate on explicit lab
fixture IDs and symbolic local event IDs. `LabContactSession` supplies the
separate volatile, invented application-selection demonstration; reset
does not change persisted units. The utility does not listen on a socket
or translate these results into historical commands or approval codes.

### Read-only contact snapshot

The `contact_snapshot` request exports a point-in-time, invented laboratory
contact fixture. It uses either generic synchronous read-only memory or the
generic read-only T=0 or T=1 application, not a historical purse application.
It contains no authorization state and
cannot change the SQLite ledger. For example, after creating `lab-example`,
save the output of a request with this content to a local fixture file:

```json
{"operation":"contact_snapshot","fixture_id":"lab-example","protocol":"t1"}
```

Use `"protocol":"sync"` for the same nonredeemable snapshot on the synchronous
memory contact path. There is no writable region or issuer-specific data in
this fixture. The portable package includes synchronous and asynchronous
snapshots for 50, 500 and zero minor LAB units. The untouched virtual contact
wallet selects the synchronous versions in slots 3 through 5; explicit
additional slot settings override the default catalogue.

The 39-byte object starts with ASCII `LABP`, a version byte, minor-unit
precision, a lifecycle code, and four big-endian unsigned 64-bit values:
balance, reserved, available and snapshot revision. Out-of-range values
are rejected, not truncated. The fixture is immutable until explicitly
exported again. The native `smartcard_async_probe.py --protocol purse-snapshot`
reads all 39 bytes across chained T=1 blocks, rejects an update command,
and verifies that the separate ledger remains unchanged. This is a contact
transport qualification only, not terminal payment acceptance.

### Stored-value demonstration and native acceptance

The Millennium craft interface manual (P0883893, Issue 00.01, printed pages
4-10 through 4-12) describes the user-visible stored-value behavior. The
terminal displays remaining value when it accepts the card, prompts for a
number if one has not been dialed, and displays the initial and subsequent
rates for a metered call. A flat-rate call has no countdown. On hangup or
Next Call the terminal briefly shows the remaining value and prompts for
card removal when appropriate. Empty cards and interrupted calls are also
part of native qualification.

For an independent ledger demonstration, seed three distinct nonredeemable
fixtures: 50, 500 and zero minor LAB units. The 50-unit fixture can represent
two simulated quarter-priced local calls. Reserve and complete each
25-unit simulated spend once; verify 50, 25 and zero remaining units, and
reject a third spend. Export new immutable read-only contact snapshots to
observe current lab balances without changing the ledger. The generic
contact transport and these accounting outcomes do not establish native
GPM103 payment application compatibility. A native balance-display test
must use an authorized reference card and its legitimate test profile, and
observe the actual terminal display, call result and reconciled records.

The [machine-readable vectors](../fixtures/cards/lab-purse-vectors.json)
cover balance, reservation, completion, event idempotency, cancellation,
reload limit, uncertain restart, inactive/empty fixtures, invalid input,
isolation and corrupt state. Additional tests cover local-journal
reconciliation, concurrent reservation, history pagination and unrelated
contact selection. Native historical application qualification still
requires authoritative reference equipment and its legitimate test
application specification.
