# Native cashbox and validator qualification

The later board now reads independent active-high cover-open and box-removed
inputs from `0xC0` bits 2 and 3. The earlier board exposes cover and box
presence through active-low status at `0x80` bits 6 and 7. These are read
signals only; the later MACH bank-selection write latch is unchanged.
Separate clickable cashbox presence and full-sensor indicators supplement
the existing vault-cover indicator.

The validator retains the physical escrow while its modeled gate travels,
transfers coins to the physical box or refund chute only when the operation
completes, rejects additional inserts during travel, and exposes the full
sensor in the serial status byte. The model keeps escrow on a blocked
collection and reports an escrow fault rather than silently creating a
successful physical collection. Removing the box transfers its modeled
contents to the removed-contents counter; replacing it starts an empty
physical box. The actual capacity and mechanical obstruction sequence
remain unmeasured. The existing two-second gate timing is a model timing,
not a measured universal mechanical duration.

The four physical amounts (escrow, box, refund and removed contents) are
stored in a separate versioned device NVRAM file. A controller reset
cancels unfinished gate motion while retaining the escrow; it does not
alter the terminal's accounting, installation flags or battery tables.
The original firmware retains ownership of its collection records, alarms,
collector identification and upload decisions.

The native isolated probe passed earlier- and later-board cover/removal
reads. The later 600 baud validator UART received actual byte-level wake,
acceptance, coin events, travel and status responses, including full and
jam indications. The synthetic sequence collected 15 test cents, blocked
a further 25-cent collection into a full box, removed the original box,
refunded the retained 25 cents and cleared a jam. The resulting physical
contents were 0 escrow, 0 box, 25 refunded and 15 removed; a separate
warm launch retained the exact saved device file. Evidence:
`build/firmware-acceptance/coin-cashbox-probe/20261126T022503.808556Z/`.
This qualifies the native hardware boundary; the installed firmware's
collection report and NCC reconciliation are separate acceptance gates.
