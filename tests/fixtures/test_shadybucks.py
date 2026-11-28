# SPDX-License-Identifier: GPL-2.0-or-later
import json
from pathlib import Path
import sys

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from shadybucks import PRODUCT, SNAPSHOT, ShadyBucks, card_identity, main


def test_independent_product_and_snapshot_lifecycle(tmp_path):
    product = json.loads((ROOT / "fixtures/cards/shadybucks-lab-product.json").read_text())
    assert product["name"] == PRODUCT and product["nonredeemable"]
    store = tmp_path / "local.sqlite"
    issuer = ShadyBucks(store)
    initial = issuer.issue(50)
    card = initial["fixture_id"]
    other = issuer.issue(500)["fixture_id"]
    assert card != other
    initial_fixture = issuer.export_contact(card)
    assert SNAPSHOT.unpack(bytes.fromhex(initial_fixture["memory_hex"])) == (
        b"SBX1", 1, card_identity(card).bytes, 50, 0, 0)
    for event, remaining in (("call-1", 25), ("call-2", 0)):
        assert issuer.begin(card, event, 25)["disposition"] == "pending"
        issuer = ShadyBucks(store)
        assert issuer.settle(card, event)["balance_units"] == remaining
        assert issuer.settle(card, event)["balance_units"] == remaining
    assert issuer.begin(card, "call-3", 25)["error"] == "insufficient_units"
    assert issuer.history(card)["committed_event_ids"] == ["call-1", "call-2"]
    assert issuer.balance(other)["balance_units"] == 500
    assert SNAPSHOT.unpack(bytes.fromhex(issuer.export_contact(card)["memory_hex"]))[3] == 0
    assert SNAPSHOT.unpack(bytes.fromhex(initial_fixture["memory_hex"]))[3] == 50


def test_cancel_and_reject_nonlocal_identifiers(tmp_path):
    issuer = ShadyBucks(tmp_path / "local.sqlite")
    card = issuer.issue(50)["fixture_id"]
    issuer.begin(card, "cancelled", 25)
    assert issuer.cancel(card, "cancelled")["available_units"] == 50
    assert issuer.history(card)["records"] == []
    with pytest.raises(ValueError):
        issuer.balance("lab-unrelated")
    assert issuer.issue(-1)["error"] == "bad_input"


def test_cli_exports_without_overwrite(tmp_path, monkeypatch, capsys):
    store = tmp_path / "local.sqlite"
    monkeypatch.setattr(sys, "argv", ["shadybucks", "--store", str(store), "issue", "50"])
    assert main() == 0
    card = json.loads(capsys.readouterr().out)["fixture_id"]
    output = tmp_path / "card.json"
    monkeypatch.setattr(sys, "argv", ["shadybucks", "--store", str(store), "export-contact", card,
                                    "--output", str(output)])
    assert main() == 0
    original = output.read_bytes()
    assert main() == 1
    assert output.read_bytes() == original
