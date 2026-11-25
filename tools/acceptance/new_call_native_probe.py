#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Exercise a real terminal's New Call key through a local SIP dialog."""

import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import socket
import subprocess
import sys

from connected_dtmf_native_probe import ROOT, LoopbackPeer, port, wait_for


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for argument in ("mame", "firmware", "battery", "settings"):
        parser.add_argument(f"--{argument}", type=Path, required=True)
    parser.add_argument("--startup-frames", type=int, default=6000)
    parser.add_argument("--output", type=Path, default=ROOT / "build/firmware-acceptance/new-call")
    args = parser.parse_args()
    settings = json.loads(args.settings.read_text())
    dial = str(settings.get("free_call", ""))
    if not re.fullmatch(r"1?[2-9][0-9]{2}[2-9][0-9]{6}", dial):
        parser.error("A permitted free-call alias is required")
    out = args.output.resolve() / datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    out.mkdir(parents=True)
    # Avoid using a live operator terminal identity against the real NCC.
    settings["ncc_port"] = int(port())
    local_settings = out / "isolated-settings.json"
    local_settings.write_text(json.dumps(settings), encoding="utf-8")
    peer = LoopbackPeer(telephone_event=True, send_audio=True)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
        probe.bind(("127.0.0.1", 0))
        sip_bind = probe.getsockname()[1]
    jack = port()
    env = {key: value for key, value in os.environ.items() if not key.upper().startswith("COINLINE_")}
    env.update(COINLINE_LINE_PORT=jack, COINLINE_CO_MODE="sip", COINLINE_CO_TIMEOUT="0",
               COINLINE_SIP_HOST="127.0.0.1", COINLINE_SIP_PORT=str(peer.port), COINLINE_SIP_USER="lab",
               COINLINE_SIP_PASSWORD="loopback-fixture", COINLINE_SIP_MY_IP="127.0.0.1", COINLINE_SIP_LOCAL_PORT=str(sip_bind))
    command = [sys.executable, str(ROOT / "tools/acceptance/graphic_session.py"), "--mame", str(args.mame.resolve()),
               "--firmware", str(args.firmware.resolve()), "--battery", str(args.battery.resolve()),
               "--settings", str(local_settings), "--output", str(out / "terminal"), "--hardware", "multipay-v1.3",
               "--artwork", "--quiet", "--keypad-trace", "--dtmf-trace", "--snapshot-interval", "0",
               "--snapshot-native", "--external-co-port", jack, "--seconds", "320"]
    result = {"passed": False}
    native = office = commands = None
    try:
        with (out / "co.log").open("wb") as co_log, (out / "launcher.log").open("wb") as launch_log:
            office = subprocess.Popen([sys.executable, "-u", str(ROOT / "tools/line-jack/central_office.py")], env=env,
                                      cwd=out, stdout=co_log, stderr=subprocess.STDOUT)
            native = subprocess.Popen(command, cwd=ROOT, env=env, stdout=launch_log, stderr=subprocess.STDOUT)

            def get_commands():
                if native.poll() is not None:
                    raise RuntimeError(f"Native launcher exited {native.returncode}")
                return next(iter((out / "terminal").glob("*/commands.txt")), None)

            commands = wait_for(get_commands, 20, "native command file")
            commands.write_text(f"wait {args.startup_frames}\nhook off\nwait 180\nkeyseq {dial}\n", encoding="ascii")
            peer.wait_request(b"INVITE", timeout=240)
            peer.wait_request(b"ACK", timeout=10)
            result["first_call_answered"] = True
            with commands.open("a", encoding="ascii") as file:
                file.write("wait 360\nsnapshot before-new-call\nkeyseq N\nwait 180\nsnapshot after-new-call\n")
            peer.wait_request(b"BYE", timeout=30)
            result["new_call_sent_bye"] = True
            with commands.open("a", encoding="ascii") as file:
                file.write(f"wait 180\nkeyseq {dial}\n")
            peer.wait_request(b"INVITE", timeout=30)
            peer.wait_request(b"ACK", timeout=10)
            result["second_call_answered"] = True
            result["passed"] = True
    except Exception as error:
        result["error"] = str(error)
    finally:
        if native is not None and native.poll() is None:
            if commands is not None:
                with commands.open("a", encoding="ascii") as file:
                    file.write("hook on\nwait 120\nexit\n")
            try:
                native.wait(timeout=25)
            except subprocess.TimeoutExpired:
                native.terminate()
                native.wait(timeout=5)
        if office is not None and office.poll() is None:
            try:
                office.wait(timeout=5)
            except subprocess.TimeoutExpired:
                office.terminate()
                office.wait(timeout=5)
        peer.close()
        if native is not None:
            result["native_exit_code"] = native.returncode
            result["passed"] &= native.returncode == 0
        (out / "result.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(f"Native New Call evidence: {out}\n{json.dumps(result, indent=2)}")
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
