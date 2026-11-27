# SPDX-License-Identifier: GPL-2.0-or-later
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import sqlite3
import struct
import sys

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from lab_purse import LabPurseStore, LabContactSession, MODEL

VECTORS = json.loads((ROOT / "fixtures/cards/lab-purse-vectors.json").read_text(encoding="utf-8"))


@pytest.mark.parametrize("name,units", [("50", 50), ("500", 500), ("empty", 0)])
@pytest.mark.parametrize("kind,protocol", [("sync", "sync"), ("async", "t1")])
def test_bundled_generic_contact_snapshots_match_nonredeemable_ledger(tmp_path, name, units, kind, protocol):
    store = LabPurseStore(tmp_path / "purses.sqlite")
    assert store.seed("lab-demo", initial_units=units, limit_units=max(50, units))["balance_units"] == units
    before = store.inspect("lab-demo")
    expected = store.contact_snapshot("lab-demo", protocol)
    fixture = json.loads((ROOT / f"fixtures/cards/smartcard-{kind}-lab-purse-{name}.json").read_text(encoding="utf-8"))
    assert fixture == expected
    assert fixture["application"] == "lab-readonly"
    assert not fixture.get("sync_program_enabled", False)
    assert store.inspect("lab-demo") == before


@pytest.mark.parametrize("scenario", VECTORS["scenarios"], ids=lambda s: s["id"])
def test_published_lab_vectors(tmp_path, scenario):
    path = tmp_path / "purses.sqlite"
    store = LabPurseStore(path)
    fixtures = scenario.get("fixture_ids", ["lab-fixture"])
    defaults = {**VECTORS["defaults"], **scenario.get("override", {})}
    for fixture in fixtures:
        assert "error" not in store.seed(fixture, **defaults)
    session = LabContactSession(); session.insert(); assert session.select(MODEL)
    for step in scenario["steps"]:
        request = {k: v for k, v in step.items() if k not in {"operation", "expect"}}
        fixture = request.pop("fixture_id", fixtures[0]); operation = step["operation"]
        if operation == "restart":
            store = LabPurseStore(path); result = store.inspect(fixture)
        elif operation == "contact_reset":
            session.reset(); assert not session.contact_present and session.selected_model is None
            result = store.inspect(fixture)
        elif operation == "restart_with_invalid_snapshot":
            with sqlite3.connect(path) as db:
                raw = db.execute("SELECT snapshot FROM fixtures WHERE id=?", (fixture,)).fetchone()[0]
                snapshot = json.loads(raw); snapshot["purse"]["balance_units"] += 1
                corrupted = json.dumps(snapshot)
                db.execute("UPDATE fixtures SET snapshot=? WHERE id=?", (corrupted, fixture))
            store = LabPurseStore(path); result = store.inspect(fixture)
            with sqlite3.connect(path) as db:
                assert db.execute("SELECT snapshot FROM fixtures WHERE id=?", (fixture,)).fetchone()[0] == corrupted
        else:
            result = getattr(store, operation)(fixture, **request)
        assert all(result.get(key) == value for key, value in step["expect"].items()), (step, result)


def test_uncertainty_reconciles_only_from_immutable_local_journal(tmp_path):
    store = LabPurseStore(tmp_path / "purses.sqlite")
    assert store.seed("lab-one")["revision"] == 0
    assert store.begin_spend("lab-one", "s1", 30)["reserved_units"] == 30
    assert store.mark_uncertain("lab-one", "s1", "local interruption")["disposition"] == "uncertain"
    assert store.complete_spend("lab-one", "s1")["error"] == "invalid_transition"
    assert store.reconcile_local("lab-one", "s1", "missing")["reserved_units"] == 30
    assert store.record_local_outcome("lab-one", "s1", "journal1", "committed")["journal_ref"] == "journal1"
    assert store.record_local_outcome("lab-one", "s1", "journal1", "cancelled")["error"] == "conflict"
    store = LabPurseStore(store.path)
    result = store.reconcile_local("lab-one", "s1", "journal1")
    assert result["balance_units"] == 70 and result["reserved_units"] == 0
    assert result["balance_display"] == "0.70 LAB" and result["available_display"] == "0.70 LAB"
    assert store.reconcile_local("lab-one", "s1", "journal1")["revision"] == result["revision"]
    assert store.seed("lab-one")["error"] == "conflict"


def test_concurrent_spends_reserve_atomically(tmp_path):
    store = LabPurseStore(tmp_path / "purses.sqlite")
    store.seed("lab-one")
    with ThreadPoolExecutor(max_workers=2) as pool:
        results = list(pool.map(lambda eid: store.begin_spend("lab-one", eid, 75), ["a", "b"]))
    assert sorted(r["disposition"] for r in results) == ["declined", "pending"]
    assert store.inspect("lab-one")["reserved_units"] == 75


def test_history_pagination_and_unrelated_contact(tmp_path):
    store = LabPurseStore(tmp_path / "purses.sqlite")
    store.seed("lab-one")
    for event in ("a", "b", "c"):
        store.begin_spend("lab-one", event, 1); store.complete_spend("lab-one", event)
    first = store.history("lab-one", limit=2)
    assert first["committed_event_ids"] == ["a", "b"]
    assert store.history("lab-one", after_sequence=first["next_cursor"])["committed_event_ids"] == ["c"]
    contact = LabContactSession(); assert not contact.select(MODEL)
    contact.insert(); assert not contact.select("unrelated-application") and contact.selected_model is None
    assert contact.select(MODEL)
    contact.reset(); assert store.inspect("lab-one")["balance_units"] == 97


def test_readonly_lab_contact_snapshot_is_fixed_and_nonmutating(tmp_path):
    store = LabPurseStore(tmp_path / "purses.sqlite")
    store.seed("lab-one")
    store.begin_spend("lab-one", "reserve", 25)
    before = store.inspect("lab-one")
    fixture = store.contact_snapshot("lab-one")
    assert fixture["application"] == "lab-readonly" and fixture["atr_hex"] == "3B 80 01 81"
    assert struct.unpack(">4sBBBQQQQ", bytes.fromhex(fixture["memory_hex"])) == (b"LABP", 1, 2, 0, 100, 25, 75, 1)
    assert store.inspect("lab-one") == before
    store.complete_spend("lab-one", "reserve")
    after = store.contact_snapshot("lab-one", "t0")
    assert after["atr_hex"] == "3B 00"
    assert struct.unpack(">4sBBBQQQQ", bytes.fromhex(after["memory_hex"])) == (b"LABP", 1, 2, 0, 75, 0, 75, 2)
    assert fixture["lab_snapshot_revision"] == 1
    assert store.contact_snapshot("lab-missing")["error"] == "unknown_fixture"
    assert store.contact_snapshot("lab-one", "other")["error"] == "bad_input"
    assert "error" not in store.seed("lab-overflow", initial_units=2**64, limit_units=2**64)
    assert store.contact_snapshot("lab-overflow")["error"] == "contact_value_range"
