# Short synchronous memory contact regression

The portable fixture catalogue includes a separate 13-byte non-payment
contact test image. The existing generic synchronous memory model required
no changes for this length.

Both the earlier and later board mappings passed complete bit-order reads,
address seeks, released out-of-range reads, short-pulse rejection, bounded
one-way programming, removal during the pending write, reinsertion and
restart persistence. The existing 16-byte GPM103-family contact fixture
passed the same suite without modification.
All 16 portable profile and fixture tests passed, including the fixture
geometry and package staging assertions.

Native executable SHA-256:
`b65f84060cbca2d1a7efa090770fbb6336709c30a38112388706334d11ef276c`.

Evidence directories (cold and warm logs for both board mappings):

- `build/firmware-acceptance/sync-13byte-bench-20261127/20261128T003313.601819Z/`
- `build/firmware-acceptance/gpm103-contact-regression-20261127/20261128T003313.512139Z/`

The test image is arbitrary and contains no application-specific data.
Issuer-table interoperability, application checks, unit encoding, debit,
exhaustion, and family-specific erase/carry semantics are not qualified by
these transport runs.

## Component regression follow-up

The synchronous model now keeps the compatibility character FIFO idle and
rejects asynchronous negotiation for a synchronous fixture, even when its
unused ATR placeholder resembles a valid serial answer. An explicit
synchronous fallback remains available. Separate component cases confirm
that loss of supply and contact deselection cancel a pending programming
pulse without modifying arbitrary test memory, including when the minimum
programming interval has already elapsed.

Fresh MSYS2/MINGW64 component build: all three selected CTest cases passed
(`test_smartcard_protocol_negotiation`, `test_smartcard_async_transport`,
`test_terminal_full_stack_vectors`). Evidence:
`build/sync-contact-component-20261127-v2/Testing/Temporary/LastTest.log`.
This is component evidence, not a new native emulator or application run.
