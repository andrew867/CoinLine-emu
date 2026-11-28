#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Independent, nonredeemable lab purse. Not an ISO card or payment protocol."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import sqlite3
import struct

MODEL = "coinline-demo-purse-v1"
LIFECYCLES = {"available", "blocked", "expired", "retired"}
DISPOSITIONS = {"pending", "committed", "cancelled", "declined", "uncertain"}
CONTACT_SNAPSHOT = struct.Struct(">4sBBBQQQQ")
CONTACT_LIFECYCLES = {"available": 0, "blocked": 1, "expired": 2, "retired": 3}


class InvalidSnapshot(ValueError):
    pass


def integer(value, minimum=0):
    return type(value) is int and value >= minimum


def identifier(value, fixture=False):
    return isinstance(value, str) and bool(re.fullmatch(
        r"lab-[A-Za-z0-9_.-]{1,120}" if fixture else r"[A-Za-z0-9_.-]{1,128}", value))


def fingerprint(kind, units):
    return hashlib.sha256(json.dumps([kind, units], separators=(",", ":")).encode("ascii")).hexdigest()


def reserved(snapshot):
    return sum(e["units"] for e in snapshot["ledger"] if e["kind"] == "spend" and e["disposition"] in {"pending", "uncertain"})


def validate(snapshot, fixture_id):
    """Reject malformed or inconsistent durable state; never regenerate it."""
    try:
        p = snapshot["purse"]
        if (snapshot["model_version"] != 1 or type(snapshot["model_version"]) is not int
                or not integer(snapshot["revision"]) or p["fixture_id"] != fixture_id
                or not identifier(fixture_id, True) or p["model_id"] != MODEL
                or p["unit"] != "LAB" or not integer(p["minor_digits"]) or p["minor_digits"] > 9
                or not integer(p["initial_units"]) or not integer(p["balance_units"])
                or not integer(p["limit_units"], 1) or p["initial_units"] > p["limit_units"]
                or p["balance_units"] > p["limit_units"] or p["lifecycle"] not in LIFECYCLES):
            raise InvalidSnapshot("invalid purse identity or values")
        ledger, incidents = snapshot["ledger"], snapshot["incidents"]
        if not isinstance(ledger, list) or not isinstance(incidents, list):
            raise InvalidSnapshot("invalid journal")
        events, sequences = set(), set()
        balance, last = p["initial_units"], 0
        for event in ledger:
            eid, kind, units, disposition, seq = (event[k] for k in ("event_id", "kind", "units", "disposition", "sequence"))
            if (not identifier(eid) or eid in events or kind not in {"spend", "reload"}
                    or not integer(units, 1) or disposition not in DISPOSITIONS
                    or (kind == "reload" and disposition != "committed")
                    or (disposition == "declined" and event.get("reason") not in {"inactive", "insufficient_units"})
                    or not integer(seq, 1) or seq <= last or event["request_fingerprint"] != fingerprint(kind, units)):
                raise InvalidSnapshot("invalid ledger event")
            events.add(eid); sequences.add(seq); last = seq
            if disposition == "committed":
                balance += units if kind == "reload" else -units
        incident_ids, incident_events, last = set(), set(), 0
        for incident in incidents:
            iid, eid, reason, seq = (incident[k] for k in ("incident_id", "related_event_id", "reason", "sequence"))
            if (not identifier(iid) or iid in incident_ids or eid not in events or not isinstance(reason, str)
                    or not reason or not integer(seq, 1) or seq <= last or seq in sequences):
                raise InvalidSnapshot("invalid incident")
            incident_ids.add(iid); incident_events.add(eid); sequences.add(seq); last = seq
        if any(e["disposition"] == "uncertain" and e["event_id"] not in incident_events for e in ledger):
            raise InvalidSnapshot("uncertain event lacks an incident")
        if balance != p["balance_units"] or reserved(snapshot) > balance or snapshot["revision"] < len(ledger):
            raise InvalidSnapshot("balance, reservation or revision invariant failed")
    except (KeyError, TypeError, ValueError) as error:
        raise InvalidSnapshot(str(error)) from error


def inspect_snapshot(snapshot):
    p = snapshot["purse"]
    held = reserved(snapshot)
    def display(units):
        scale = 10 ** p["minor_digits"]
        whole, fraction = divmod(units, scale)
        amount = str(whole) if p["minor_digits"] == 0 else f"{whole}.{fraction:0{p['minor_digits']}d}"
        return f"{amount} {p['unit']}"
    return {**p, "reserved_units": held, "available_units": p["balance_units"] - held,
            "balance_display": display(p["balance_units"]), "available_display": display(p["balance_units"] - held),
            "revision": snapshot["revision"]}


def sequence(snapshot):
    return 1 + max((e["sequence"] for e in snapshot["ledger"] + snapshot["incidents"]), default=0)


def find_event(snapshot, event_id):
    return next((e for e in snapshot["ledger"] if e["event_id"] == event_id), None)


class LabPurseStore:
    """Local SQLite snapshots, with transaction-scoped mutation and validation."""

    def __init__(self, path):
        self.path = Path(path)
        with sqlite3.connect(self.path) as db:
            db.execute("CREATE TABLE IF NOT EXISTS fixtures(id TEXT PRIMARY KEY, snapshot TEXT NOT NULL)")
            db.execute("CREATE TABLE IF NOT EXISTS local_journal(fixture TEXT, ref TEXT, event TEXT, fingerprint TEXT, "
                       "disposition TEXT, PRIMARY KEY(fixture, ref), UNIQUE(fixture, event))")

    def seed(self, fixture_id, model_id=MODEL, unit="LAB", minor_digits=2, initial_units=100,
             limit_units=200, lifecycle="available"):
        snapshot = {"model_version": 1, "revision": 0, "purse": {"fixture_id": fixture_id, "model_id": model_id,
                    "unit": unit, "minor_digits": minor_digits, "initial_units": initial_units,
                    "balance_units": initial_units, "limit_units": limit_units, "lifecycle": lifecycle},
                    "ledger": [], "incidents": []}
        try:
            validate(snapshot, fixture_id)
        except InvalidSnapshot:
            return {"error": "bad_input"}
        try:
            with sqlite3.connect(self.path) as db:
                db.execute("INSERT INTO fixtures VALUES(?, ?)", (fixture_id, json.dumps(snapshot)))
            return inspect_snapshot(snapshot)
        except sqlite3.IntegrityError:
            return {"error": "conflict"}
        except sqlite3.Error:
            return {"error": "persistence_error"}

    def _operate(self, fixture_id, action):
        if not identifier(fixture_id, True):
            return {"error": "bad_input"}
        try:
            with sqlite3.connect(self.path, isolation_level="IMMEDIATE") as db:
                db.execute("BEGIN IMMEDIATE")
                row = db.execute("SELECT snapshot FROM fixtures WHERE id=?", (fixture_id,)).fetchone()
                if row is None:
                    return {"error": "unknown_fixture"}
                snapshot = json.loads(row[0]); validate(snapshot, fixture_id)
                extra, changed = action(snapshot, db)
                if changed:
                    snapshot["revision"] += 1; validate(snapshot, fixture_id)
                    db.execute("UPDATE fixtures SET snapshot=? WHERE id=?", (json.dumps(snapshot), fixture_id))
                result = {**inspect_snapshot(snapshot), **extra}
            return result
        except (sqlite3.Error, InvalidSnapshot, json.JSONDecodeError):
            return {"error": "persistence_error", "replacement_fixture_created": False}

    def inspect(self, fixture_id):
        return self._operate(fixture_id, lambda s, db: ({}, False))

    def contact_snapshot(self, fixture_id, protocol="t1"):
        """Export an immutable, invented LAB contact object, never an authorization."""
        if protocol not in ("sync", "t0", "t1"):
            return {"error": "bad_input"}
        result = self.inspect(fixture_id)
        if "error" in result:
            return result
        try:
            data = CONTACT_SNAPSHOT.pack(b"LABP", 1, result["minor_digits"],
                CONTACT_LIFECYCLES[result["lifecycle"]], result["balance_units"],
                result["reserved_units"], result["available_units"], result["revision"])
        except struct.error:
            return {"error": "contact_value_range"}
        if protocol == "sync":
            # This is an invented read-only LAB object on a synchronous
            # contact memory bus, not a historical card application.
            return {"type": "smartcard", "protocol": "sync", "atr_hex": "00",
                    "memory_hex": data.hex().upper(), "application": "lab-readonly",
                    "requires_authorization": False, "lab_snapshot_revision": result["revision"],
                    "lab_snapshot_format": "LABP-v1-nonredeemable"}
        return {"type": "smartcard", "protocol": "async", "application": "lab-readonly",
                "reset_policy": "external", "atr_hex": "3B 00" if protocol == "t0" else "3B 80 01 81",
                "memory_hex": data.hex().upper(), "atr_delay_us": 4000,
                "apdu_response_delay_us": 1000, "lab_snapshot_revision": result["revision"],
                "lab_snapshot_format": "LABP-v1-nonredeemable"}

    def history(self, fixture_id, after_sequence=0, limit=100):
        def action(s, db):
            if not integer(after_sequence) or not integer(limit, 1):
                return {"error": "bad_input"}, False
            entries = [e for e in s["ledger"] if e["disposition"] == "committed" and e["sequence"] > after_sequence]
            page = entries[:limit]
            return {"records": page, "committed_event_ids": [e["event_id"] for e in page],
                    "next_cursor": page[-1]["sequence"] if len(entries) > len(page) else None}, False
        return self._operate(fixture_id, action)

    def _new_event(self, fixture_id, event_id, units, kind):
        def action(s, db):
            if not identifier(event_id) or not integer(units, 1):
                return {"error": "bad_input"}, False
            fp = fingerprint(kind, units)
            prior = find_event(s, event_id)
            if prior:
                if prior["request_fingerprint"] != fp:
                    return {"error": "conflict"}, False
                return self._result(prior), False
            p = s["purse"]
            disposition, reason = "pending", None
            if kind == "reload":
                if p["balance_units"] + units > p["limit_units"]:
                    return {"error": "limit_exceeded"}, False
                p["balance_units"] += units; disposition = "committed"
            elif p["lifecycle"] != "available":
                disposition, reason = "declined", "inactive"
            elif units > p["balance_units"] - reserved(s):
                disposition, reason = "declined", "insufficient_units"
            event = {"event_id": event_id, "kind": kind, "units": units, "disposition": disposition,
                     "sequence": sequence(s), "request_fingerprint": fp}
            if reason:
                event["reason"] = reason
            s["ledger"].append(event)
            return self._result(event), True
        return self._operate(fixture_id, action)

    @staticmethod
    def _result(event):
        result = {"event_id": event["event_id"], "disposition": event["disposition"]}
        if event["disposition"] == "declined":
            result["error"] = event["reason"]
        return result

    def begin_spend(self, fixture_id, event_id, units):
        return self._new_event(fixture_id, event_id, units, "spend")

    def reload_fixture(self, fixture_id, event_id, units):
        return self._new_event(fixture_id, event_id, units, "reload")

    def _transition(self, fixture_id, event_id, target, reason=None):
        def action(s, db):
            if not identifier(event_id) or (target == "uncertain" and (not isinstance(reason, str) or not reason)):
                return {"error": "bad_input"}, False
            event = find_event(s, event_id)
            if event is None:
                return {"error": "unknown_event"}, False
            if event["kind"] != "spend":
                return {"error": "invalid_transition"}, False
            if event["disposition"] == target:
                if target == "uncertain" and not any(i["related_event_id"] == event_id and i["reason"] == reason for i in s["incidents"]):
                    return {"error": "conflict"}, False
                return self._result(event), False
            if event["disposition"] != "pending":
                return {"error": "invalid_transition"}, False
            event["disposition"] = target
            if target == "committed":
                s["purse"]["balance_units"] -= event["units"]
            if target == "uncertain":
                seq = sequence(s)
                s["incidents"].append({"incident_id": f"incident-{seq}", "related_event_id": event_id,
                                       "reason": reason, "sequence": seq})
            return self._result(event), True
        return self._operate(fixture_id, action)

    def complete_spend(self, fixture_id, event_id):
        return self._transition(fixture_id, event_id, "committed")

    def cancel_spend(self, fixture_id, event_id):
        return self._transition(fixture_id, event_id, "cancelled")

    def mark_uncertain(self, fixture_id, event_id, reason):
        return self._transition(fixture_id, event_id, "uncertain", reason)

    def recover_abandoned_local_spends(self, fixture_id):
        """An exclusive simulator session reopened without completion evidence."""
        def action(s, db):
            recovered = []
            for event in s["ledger"]:
                if event["kind"] == "spend" and event["disposition"] == "pending":
                    event["disposition"] = "uncertain"
                    seq = sequence(s)
                    s["incidents"].append({"incident_id": f"incident-{seq}",
                                           "related_event_id": event["event_id"],
                                           "reason": "local simulator restarted without completion evidence",
                                           "sequence": seq})
                    recovered.append(event["event_id"])
            return {"recovered_events": recovered}, bool(recovered)
        return self._operate(fixture_id, action)

    def record_local_outcome(self, fixture_id, event_id, journal_ref, disposition):
        """Simulator administration only: immutable local journal, never native I/O."""
        def action(s, db):
            if not identifier(journal_ref) or not identifier(event_id) or disposition not in {"committed", "cancelled"}:
                return {"error": "bad_input"}, False
            event = find_event(s, event_id)
            if not event:
                return {"error": "unknown_event"}, False
            row = db.execute("SELECT ref,event,fingerprint,disposition FROM local_journal WHERE fixture=? AND (ref=? OR event=?)",
                             (fixture_id, journal_ref, event_id)).fetchall()
            expected = (journal_ref, event_id, event["request_fingerprint"], disposition)
            if row:
                return ({"journal_ref": journal_ref} if row == [expected] else {"error": "conflict"}), False
            if event["kind"] != "spend" or event["disposition"] != "uncertain":
                return {"error": "invalid_transition"}, False
            db.execute("INSERT INTO local_journal VALUES(?,?,?,?,?)", (fixture_id, *expected))
            return {"journal_ref": journal_ref}, True
        return self._operate(fixture_id, action)

    def reconcile_local(self, fixture_id, event_id, journal_ref):
        def action(s, db):
            if not identifier(journal_ref) or not identifier(event_id):
                return {"error": "bad_input"}, False
            event = find_event(s, event_id)
            if not event:
                return {"error": "unknown_event"}, False
            row = db.execute("SELECT event,fingerprint,disposition FROM local_journal WHERE fixture=? AND ref=?",
                             (fixture_id, journal_ref)).fetchone()
            if not row:
                return (self._result(event) if event["disposition"] == "uncertain" else {"error": "invalid_transition"}), False
            if row[:2] != (event_id, event["request_fingerprint"]) or row[2] not in {"committed", "cancelled"}:
                return {"error": "conflict"}, False
            if event["disposition"] == row[2]:
                return self._result(event), False
            if event["disposition"] != "uncertain":
                return {"error": "invalid_transition"}, False
            event["disposition"] = row[2]
            if row[2] == "committed":
                s["purse"]["balance_units"] -= event["units"]
            return self._result(event), True
        return self._operate(fixture_id, action)


class LabContactSession:
    """Volatile independent lab UI state, deliberately not attached to native APDUs."""

    def __init__(self):
        self._sequence = 0
        self.reset()

    def insert(self):
        self._sequence += 1
        self.session_id = f"local-session-{self._sequence}"
        self.transport_state = "present"
        self.contact_present, self.selected_model = True, None

    def select(self, model_id):
        self.selected_model = MODEL if self.contact_present and model_id == MODEL else None
        self.transport_state = "selected" if self.selected_model else "present" if self.contact_present else "absent"
        return self.selected_model is not None

    def reset(self):
        self.session_id = None
        self.transport_state = "absent"
        self.contact_present, self.selected_model = False, None


class LabDisposableSession:
    """Exclusive local, nonredeemable test session; no native card protocol."""

    def __init__(self, store, fixture_id):
        self.store, self.fixture_id = store, fixture_id
        self.present = False
        self.pending = None

    def insert(self):
        if self.present:
            return self.enquire()
        result = self.store.recover_abandoned_local_spends(self.fixture_id)
        if "error" not in result:
            self.present = True
            self.pending = None
        return result

    def enquire(self):
        if not self.present:
            return {"error": "card_absent"}
        return self.store.inspect(self.fixture_id)

    def request_debit(self, event_id, units):
        if not self.present:
            return {"error": "card_absent"}
        if self.pending is not None and event_id != self.pending:
            return {"error": "busy"}
        result = self.store.begin_spend(self.fixture_id, event_id, units)
        if result.get("disposition") == "pending":
            self.pending = event_id
        return result

    def finish_debit(self, event_id, success):
        if not self.present:
            return {"error": "card_absent"}
        if event_id != self.pending or self.pending is None:
            return {"error": "no_pending_request"}
        if type(success) is not bool:
            return {"error": "bad_input"}
        operation = self.store.complete_spend if success else self.store.cancel_spend
        result = operation(self.fixture_id, event_id)
        if "error" not in result:
            self.pending = None
        return result

    def remove(self):
        # A lost completion observation is not evidence of a failed debit.
        # Keep the reservation until the independent local journal resolves it.
        self.present = False
        if self.pending is not None:
            result = self.store.mark_uncertain(self.fixture_id, self.pending, "local simulated contact removed")
            if "error" not in result:
                self.pending = None
            return result
        return {"present": False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--store", type=Path, required=True)
    parser.add_argument("--request", type=Path, required=True, help="local JSON simulator request")
    args = parser.parse_args()
    try:
        request = json.loads(args.request.read_text(encoding="utf-8"))
        operation = request.pop("operation")
        allowed = {"seed", "inspect", "contact_snapshot", "history", "begin_spend", "complete_spend", "cancel_spend", "mark_uncertain",
                   "record_local_outcome", "reconcile_local", "reload_fixture"}
        result = getattr(LabPurseStore(args.store), operation)(**request) if operation in allowed else {"error": "bad_input"}
    except sqlite3.Error:
        result = {"error": "persistence_error", "replacement_fixture_created": False}
    except (ValueError, TypeError, KeyError, OSError):
        result = {"error": "bad_input"}
    print(json.dumps(result, indent=2))
    return 1 if "error" in result else 0


if __name__ == "__main__":
    raise SystemExit(main())
