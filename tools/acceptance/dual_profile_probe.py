#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Verify two packaged terminal profiles can run and register concurrently."""

import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

from run import ROOT

sys.path.insert(0, str(ROOT / "tools/line-jack"))
from test_connected_dtmf_sip_loopback import LoopbackPeer


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package", required=True, type=Path)
    args = parser.parse_args()
    # Always use an isolated copy. Never overwrite the operator's INI or state.
    output = ROOT / "build/firmware-acceptance/dual-profile"
    output.mkdir(parents=True, exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix="probe-", dir=output))
    package = run / "package"
    shutil.copytree(args.package.resolve(), package, ignore=shutil.ignore_patterns("data", "coinline.ini", "coinline-graphical.ini", "*.log"))
    peer = LoopbackPeer()
    children = []
    logs = []
    result = {"passed": False, "registrations": {}}
    try:
        for profile, ini, sip_port in (("oem", "coinline.ini", 5087), ("graphical", "coinline-graphical.ini", 5088)):
            (package / ini).write_text(f"[sip]\nregistrar=127.0.0.1\nregistrar_port={peer.port}\n"
                                       f"username={profile}\npassword=loopback-fixture\nlocal_address=127.0.0.1\n",
                                       encoding="ascii")
            log = (run / f"{profile}.log").open("wb")
            logs.append(log)
            child = subprocess.Popen([str(package / "runtime/python.exe"), str(package / "app/launch.py"), profile,
                                      "--sip", "--headless", "--seconds", "26"], cwd=package,
                                     stdout=log, stderr=subprocess.STDOUT)
            children.append((profile, sip_port, child))
        for _ in range(6):
            _, headers, _ = peer.wait_request(b"REGISTER", timeout=15)
            for profile, sip_port, child in children:
                if (f"sip:{profile}@".encode() in headers[b"from"] and str(sip_port).encode() in headers[b"via"]):
                    result["registrations"][profile] = sip_port
            if len(result["registrations"]) == 2:
                break
        result["concurrent_at_registration"] = all(child.poll() is None for _, _, child in children)
        result["exit_codes"] = {profile: child.wait(timeout=90) for profile, _, child in children}
        result["line_jacks"] = {}
        for profile, _, _ in children:
            paths = list((package / "data" / profile).rglob("central-office.log"))
            result["line_jacks"][profile] = len(paths) == 1 and "jack C1 line present" in paths[0].read_text(errors="replace")
        result["passed"] = (result["registrations"] == {"oem": 5087, "graphical": 5088}
                            and result["concurrent_at_registration"] and all(x == 0 for x in result["exit_codes"].values())
                            and all(result["line_jacks"].values()))
    except Exception as error:
        result["error"] = str(error)
    finally:
        for _, _, child in children:
            if child.poll() is None:
                # Bounded native runs normally exit and close their CO themselves.
                try:
                    child.wait(timeout=90)
                except subprocess.TimeoutExpired:
                    child.terminate()
                    child.wait(timeout=5)
        for log in logs:
            log.close()
        peer.close()
        (run / "result.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(f"Dual profile evidence: {run}")
    print(json.dumps(result, indent=2))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
