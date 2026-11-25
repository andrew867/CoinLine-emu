#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Provision isolated INSTALL targets through the local CoinLine API."""

import argparse
import base64
from functools import cache
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
from urllib.error import HTTPError
from urllib.request import Request, urlopen

from run import json_file


# Firmware grouping follows the public manager's terminal and table lists.
LABS = (
    ("MSR 1.7 Multi-Pay", "msr17-multipay", "legacy", ("06JAA03", "12AAV08", "M06CAA17")),
    ("MSR 1.7 Generic Multi-Pay", "msr17-multipay-generic-data", "legacy", ("NAA1S05",)),
    ("MSR 1.7 Card-only", "msr17-card", "legacy-card", ("M06CBBX1",)),
    ("MSR 1.7 Desk REL 2", "msr17-desk-generic", "legacy-desk", ("NAJ2S05",)),
    ("MSR 1.7 Coin Basic", "msr17-coin-basic", "legacy-coin", ("NAD4K02", "NAD4S02")),
    ("MTR 1.9 Multi-Pay V1.0", "mtr19-multipay", "universal", ("NBA1F02", "NNA1F02")),
    ("MTR 1.9 Desk REL 1", "mtr19-desk", "universal-desk", ("NBJ1S03", "NBJ1F03")),
    ("MTR 1.13 Multi-Pay V1.0", "mtr113-multipay", "universal", ("NNK1F05",)),
    ("MTR 1.20 Multi-Pay V1.0", "mtr120-multipay", "expanded", ("NPA1S01", "NPA1F01")),
    ("MTR 1.20 Coin Basic", "mtr120-coin-basic", "expanded-coin", ("NPE1S01",)),
    ("MTR 2.0 Multi-Pay V1.0", "mtr20-multipay", "universal", ("NCA1X03",)),
    ("MTR 2.11 Multi-Pay V1.1", "mtr211-multipay", "expanded_v11", ("NKA1X02", "NKA1X05")),
    ("MTR 2.x Multi-Pay V1.3", "mtr2x-multipay", "expanded_spare", ("NLA1X05", "NOA1X04", "NOK1X03", "NQA1X01")),
)

RECORDS = {
    "legacy": (21, 22, 23, 24, 26, 31, 50, 62, 73, 74),
    "universal": (21, 22, 23, 26, 31, 50, 62, 73, 92, 93, 101),
    "expanded": (21, 26, 31, 50, 62, 73, 92, 93, 134, 135, 136),
    "expanded_spare": (21, 26, 31, 50, 62, 72, 73, 92, 93, 134, 135, 136),
    "expanded_v11": (21, 26, 31, 50, 62, 72, 73, 85, 86, 92, 93, 134, 135, 136),
}
ARCHIVE_LENGTHS = {22: 660, 23: 677, 24: 720, 26: 71, 50: 104, 62: 352, 72: 1000, 73: 1191,
                   74: 818, 85: 8000, 86: 8000, 92: 3060, 93: 224, 101: 402,
                   134: 1152, 135: 1108, 136: 202}
DEFINITIONS = {24: "Call screening list (Universal)", 72: "Spare table", 74: "NPA/NXX table 1 (uncompressed)",
               85: "Expanded visual prompts language 1", 86: "Expanded visual prompts language 2",
               134: "Expanded card table", 135: "Expanded carrier table", 136: "Expanded NPA/NXX table 1"}
FEATURE_TERMINAL_TYPE = {"card": 1, "desk": 1, "": 2, "coin": 3}


def api(port, path, body=None, missing=False, method=None):
    headers = {"Content-Type": "application/json"}
    if key := os.getenv("COINLINE_LAB_API_KEY"):
        headers["X-Api-Key"] = key
    if token := os.getenv("COINLINE_LAB_OPERATOR_TOKEN"):
        headers["Authorization"] = f"Bearer {token}"
    elif operator := os.getenv("COINLINE_LAB_DEVELOPMENT_OPERATOR"):
        headers["X-Operator-Id"] = operator
    request = Request(f"http://127.0.0.1:{port}/api/{path}",
                      data=json.dumps(body).encode() if body is not None else None, headers=headers,
                      method=method)
    try:
        with urlopen(request, timeout=30) as response:
            raw = response.read()
            return json.loads(raw) if raw else None
    except HTTPError as error:
        if missing and error.code == 404:
            return None
        raise ValueError(f"CoinLine {request.get_method()} {path}: HTTP {error.code}: "
                         + error.read().decode(errors="replace")) from error


@cache
def read_archive(repo, commit, number):
    name = f"tables/default/mm_table_{number:02x}.bin"
    result = subprocess.run(["git", "-C", str(repo), "show", f"{commit}:{name}"],
                            check=True, capture_output=True)
    raw = result.stdout
    if len(raw) != ARCHIVE_LENGTHS[number]:
        raise ValueError(f"Unexpected {name} length at the selected source revision")
    return raw


def encode_craft_access(digits):
    if not isinstance(digits, str) or not re.fullmatch(r"[0-9]{7}", digits):
        raise ValueError("The synthetic Craft access code must contain seven digits")
    return bytes.fromhex(digits + "e")


def payloads(repo, commit, recipe, line, craft_access, desk_key):
    layout, _, model = recipe.partition("-")
    expanded = layout in ("expanded", "expanded_spare", "expanded_v11")
    width = 10 if expanded else 6
    ncc = bytes.fromhex("2130706433".replace("0", "a")).ljust(width, b"\0")
    access = bytes([21]) + bytes.fromhex(line) + ncc + bytes(width + (22 if expanded else 0))
    install = bytearray(37)
    install[0] = 31
    install[1:5] = encode_craft_access(craft_access)
    install[5:10] = bytes.fromhex(desk_key)
    install[11:17] = bytes([10, 0, 40, 0, 150, 0])
    install[17] = install[21] = 10
    records = {}
    for number in RECORDS[layout]:
        if (model in ("desk", "card") and number == 50) or (model == "coin" and number in (22, 134)):
            continue
        if number == 21:
            records[number] = access
        elif number == 31:
            records[number] = bytes(install)
        else:
            raw = read_archive(repo, commit, number)
            if number == 26:
                raw = bytes([FEATURE_TERMINAL_TYPE[model]]) + raw[1:]
            if number == 92 and layout == "universal":
                raw += bytes(20 * 17)
            records[number] = bytes([number]) + raw
    return records


def provision(args):
    settings = json.loads(args.settings.read_text(encoding="utf-8"))
    pin = settings.get("pin")
    if not isinstance(pin, str) or not re.fullmatch(r"[0-9]{5}", pin):
        raise ValueError("The acceptance PIN must contain five digits")
    craft_access = settings.get("craft_access", "2727378")
    encode_craft_access(craft_access)
    desk_key = "0000000000"
    if "desk_key_card" in settings:
        track = json.loads(Path(settings["desk_key_card"]).read_text(encoding="utf-8"))["payload_body"]
        match = re.match(r";([0-9]{10})[0-9]*=", track)
        if not match:
            raise ValueError("The synthetic Desk key card must contain at least ten track-2 digits")
        desk_key = match[1]
    commit = subprocess.check_output(["git", "-C", str(args.manager), "rev-parse", "HEAD"], text=True).strip()
    plans = []
    for index, (label, family, recipe, codes) in enumerate(LABS):
        if args.only and not set(codes).intersection(args.only):
            continue
        if args.apply and recipe.endswith("-desk") and "desk_key_card" not in settings:
            raise ValueError("Set desk_key_card to a synthetic Desk key-card fixture before provisioning Desk targets")
        line = str(int(args.first_line) + index)
        plans.append((label, family, line, codes, payloads(args.manager, commit, recipe, line, craft_access, desk_key)))
    if not plans:
        raise ValueError("No requested firmware has a lab recipe")

    def host(path, body=None, missing=False, method=None):
        return api(args.port, path, body, missing, method)

    phones = host("terminals")
    anchor = next((phone for phone in phones if phone["displayName"] == args.anchor), None)
    if anchor is None:
        raise ValueError("The requested anchor terminal was not found")
    families = {family["id"] for family in host("terminals/families")}
    definitions = {row["tableNumber"]: row["id"] for row in host("tables/definitions")}
    versions = {row["label"]: row for row in host("firmware/versions")}
    sets = {row["name"]: row for row in host("tables/sets")}
    overrides = settings.setdefault("firmware_overrides", {})
    report_path = args.settings.with_name("ncc-lab-provisioning.json")
    previous = json.loads(report_path.read_text(encoding="utf-8")) if report_path.is_file() else {}
    results = {target["terminal"]: target for target in previous.get("targets", [])}
    for label, family, line, codes, records in plans:
        name = f"Emulator acceptance {label}"
        phone = next((row for row in phones if row["displayName"] == name), None)
        if any(row["terminalIdHex"] == line and row["displayName"] != name for row in phones):
            raise ValueError(f"The requested synthetic wire ID for {name} is already in use")
        if phone and (phone["terminalIdHex"] != line or phone["siteId"] != anchor["siteId"]):
            raise ValueError(f"The existing {name} has a different identity or site")
        firmware = versions.get(name)
        if phone and (not firmware or phone.get("firmwareVersionId") != firmware["id"]):
            raise ValueError(f"The existing {name} has a different firmware assignment")
        layout_family = family if family in families else None
        if phone and phone.get("terminalFamilyId") not in (None, layout_family):
            raise ValueError(f"The existing {name} has a different terminal family")
        print(f"{name} [{layout_family or 'firmware inventory only'}]", flush=True)
        if not args.apply:
            continue
        if not firmware:
            firmware = host("firmware/versions", {"label": name, "buildId": ", ".join(codes)})
        if not phone:
            phone = host("terminals", {"siteId": anchor["siteId"], "firmwareVersionId": firmware["id"],
                                       "terminalIdHex": line, "displayName": name, "status": 1,
                                       "terminalFamilyId": layout_family})
        elif layout_family and phone.get("terminalFamilyId") is None:
            host(f"terminals/{phone['id']}/family", {"terminalFamilyId": layout_family}, method="PUT")

        expected = {number: hashlib.sha256(raw).hexdigest() for number, raw in records.items()}
        set_digest = hashlib.sha256(b"".join(records.values())).hexdigest()[:12]
        set_name = f"{name} {set_digest}"
        desired = sets.get(set_name)
        if not desired:
            desired = host("tables/sets", {"name": set_name, "isDefault": False,
                                           "targetFirmwareVersionId": firmware["id"]})
        record_set = host(f"tables/sets/{desired['id']}")
        if record_set["targetFirmwareVersionId"] != firmware["id"]:
            raise ValueError(f"{set_name} targets a different firmware version")
        if record_set["status"] == 0:
            for order, (number, raw) in enumerate(records.items()):
                if number not in definitions:
                    new = host("tables/definitions", {"name": DEFINITIONS[number], "tableNumber": number})
                    definitions[number] = new["id"]
                body = {"tableSetId": desired["id"], "tableDefinitionId": definitions[number], "tableRevision": 1,
                        "sortOrder": order, "payloadBase64": base64.b64encode(raw).decode()}
                if not host("tables/versions", body)["validationPassed"]:
                    raise ValueError(f"CoinLine rejected MT{number} for {set_name}")
            host(f"tables/sets/{desired['id']}/publish?confirm=true", {})
            record_set = host(f"tables/sets/{desired['id']}")
        delivered = {row["tableNumber"]: row["payloadSha256Hex"].lower() for row in record_set["versions"]
                     if row["validationPassed"]}
        if record_set["status"] != 1 or delivered != expected or len(record_set["versions"]) != len(expected):
            raise ValueError(f"Published records do not match the prepared data for {set_name}")
        route = f"terminals/{phone['id']}/table-assignment"
        active = host(route, missing=True)
        if not active or active["tableSetId"] != desired["id"]:
            host(route, {"tableSetId": desired["id"]})
        if host(route)["tableSetId"] != desired["id"]:
            raise ValueError(f"CoinLine did not retain the requested assignment for {name}")
        for code in codes:
            overrides.setdefault(code, {}).update(line=line)
        results[name] = {"terminal": name, "terminal_id": phone["id"], "family": layout_family,
                         "firmware_ids": codes, "set_id": desired["id"], "record_sha256": expected,
                         "manager_commit": commit}
        json_file(args.settings, settings)
        json_file(report_path, {"targets": list(results.values())})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--settings", type=Path, default=Path("build/firmware-acceptance/settings.json"))
    parser.add_argument("--manager", type=Path, default=Path("build/firmware-acceptance/sources/manager"))
    parser.add_argument("--anchor", required=True, help="existing terminal at the lab site")
    parser.add_argument("--first-line", default="2025550100", help="first fictional NANP number in the lab range")
    parser.add_argument("--port", type=int, default=5006, help="CoinLine HTTP port on loopback")
    parser.add_argument("--only", action="append", help="provision only labs containing this firmware ID")
    parser.add_argument("--apply", action="store_true", help="create or update only emulator acceptance targets")
    args = parser.parse_args()
    if not re.fullmatch(r"[2-9][0-9]{2}55501[0-9]{2}", args.first_line) or int(args.first_line[-2:]) + len(LABS) > 100:
        parser.error("--first-line must leave enough numbers in a fictional NANP 555-0100 through 555-0199 range")
    try:
        provision(args)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"{error}\n")


if __name__ == "__main__":
    main()
