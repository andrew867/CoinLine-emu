#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Independent ShadyBucks demonstration issuer and local transaction ledger."""

import argparse
import json
from pathlib import Path
import struct
import uuid

from lab_purse import LabPurseStore


PRODUCT = "ShadyBucks Stored Value Smart Card"
PREFIX = "lab-sb-"
SNAPSHOT = struct.Struct(">4sB16sQQQ")


def card_identity(card):
    if not isinstance(card, str) or not card.startswith(PREFIX):
        raise ValueError("not a ShadyBucks laboratory card identifier")
    value = uuid.UUID(card[len(PREFIX):])
    if card != PREFIX + value.hex:
        raise ValueError("invalid ShadyBucks laboratory card identifier")
    return value


class ShadyBucks:
    """Local demonstration product: no native issuer protocol or external funds."""

    def __init__(self, path):
        self.store = LabPurseStore(path)

    def issue(self, units):
        if type(units) is not int or not 0 <= units < 2**63:
            return {"error": "bad_input"}
        card = PREFIX + uuid.uuid4().hex
        return self._label(self.store.seed(card, initial_units=units, limit_units=max(1, units)))

    @staticmethod
    def _label(result):
        if "error" not in result:
            return {**result, "product": PRODUCT, "unit_display": "ShadyBucks"}
        return result

    def balance(self, card):
        card_identity(card)
        return self._label(self.store.inspect(card))

    def begin(self, card, event, units):
        card_identity(card)
        return self._label(self.store.begin_spend(card, event, units))

    def settle(self, card, event):
        card_identity(card)
        return self._label(self.store.complete_spend(card, event))

    def cancel(self, card, event):
        card_identity(card)
        return self._label(self.store.cancel_spend(card, event))

    def history(self, card):
        card_identity(card)
        return self._label(self.store.history(card))

    def export_contact(self, card):
        identity = card_identity(card)
        state = self.balance(card)
        if "error" in state:
            return state
        # A new, independently defined read-only object for this issuer. It
        # is not a native terminal card format or an authorization response.
        data = SNAPSHOT.pack(b"SBX1", 1, identity.bytes, state["balance_units"],
                             state["reserved_units"], state["revision"])
        return {"type": "smartcard", "protocol": "sync", "atr_hex": "00",
                "fixture_kind": "shadybucks-lab-v1", "memory_hex": data.hex().upper()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--store", type=Path, required=True)
    commands = parser.add_subparsers(dest="operation", required=True)
    commands.add_parser("issue").add_argument("units", type=int)
    for name in ("balance", "history", "export-contact", "begin", "settle", "cancel"):
        operation = commands.add_parser(name)
        operation.add_argument("card")
        if name in {"begin", "settle", "cancel"}:
            operation.add_argument("event")
        if name == "begin":
            operation.add_argument("units", type=int)
        if name == "export-contact":
            operation.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        issuer = ShadyBucks(args.store)
        values = vars(args).copy()
        operation = values.pop("operation").replace("-", "_")
        values.pop("store")
        output = values.pop("output", None)
        result = getattr(issuer, operation)(**values)
        if output is not None and "error" not in result:
            # Export creates a new snapshot, never overwrites a fixture.
            with output.open("x", encoding="utf-8") as file:
                json.dump(result, file, indent=2)
                file.write("\n")
            result = {"fixture": output.name, "format": "SBX1"}
    except (ValueError, OSError):
        result = {"error": "bad_input"}
    print(json.dumps(result, indent=2))
    return int("error" in result)


if __name__ == "__main__":
    raise SystemExit(main())
