#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Prove connected keypad DTMF passes from real firmware to a loopback SIP peer."""

import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import socket
import struct
import subprocess
import sys
import time

from run import ROOT, port

sys.path.insert(0, str(ROOT / "tools/line-jack"))
from test_connected_dtmf_sip_loopback import LoopbackPeer


def wait_for(predicate, timeout, label):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        if value := predicate():
            return value
        time.sleep(0.1)
    raise RuntimeError(f"Timed out waiting for {label}")


def read_log(path):
    try:
        return path.read_text(encoding="utf-8", errors="replace")
    except (FileNotFoundError, PermissionError):
        return ""


def connected_digits(log):
    # MAME's end-of-run speed banner can be written in the middle of a
    # buffered stderr diagnostic because the launcher merges both streams.
    # Remove only that complete banner, retaining every hardware gate field.
    log = re.sub(r"Average speed: [^\r\n]*\r?\n", "", log)
    return re.findall(r"^DTMF pad make [0-9.]+ ([0-9*#]) enabled=1 .* line=1$", log, re.M)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mame", required=True, type=Path)
    parser.add_argument("--firmware", required=True, type=Path)
    parser.add_argument("--battery", required=True, type=Path, help="copy of a completed graphical installation")
    parser.add_argument("--settings", required=True, type=Path, help="lab settings with a permitted free_call")
    parser.add_argument("--startup-frames", type=int, default=6000, help="emulated warm-maintenance allowance before lifting the handset")
    parser.add_argument("--answer-frames", type=int, default=360, help="emulated answer-supervision allowance before connected keypad stimulus")
    parser.add_argument("--output", type=Path, default=ROOT / "build/firmware-acceptance/connected-dtmf-sip-native")
    args = parser.parse_args()
    if min(args.startup_frames, args.answer_frames) < 0:
        parser.error("frame allowances must be nonnegative")
    settings = json.loads(args.settings.read_text())
    dial = str(settings.get("free_call", ""))
    if not re.fullmatch(r"1?[2-9][0-9]{2}[2-9][0-9]{6}", dial):
        parser.error("Lab settings must contain a permitted North American free_call alias")

    out = args.output.resolve() / datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    out.mkdir(parents=True)
    peer = LoopbackPeer(telephone_event=True, info=True, send_audio=True)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
        probe.bind(("127.0.0.1", 0))
        sip_bind = probe.getsockname()[1]
    jack = port()
    # The fake registrar is bound to loopback. No account/registrar data or
    # network routes from the operator's environment are inherited.
    env = {key: value for key, value in os.environ.items() if not key.upper().startswith("COINLINE_")}
    env.update(COINLINE_LINE_PORT=jack, COINLINE_CO_MODE="sip", COINLINE_CO_TIMEOUT="0", COINLINE_SIP_HOST="127.0.0.1",
               COINLINE_SIP_PORT=str(peer.port), COINLINE_SIP_USER="lab", COINLINE_SIP_PASSWORD="loopback-fixture",
               COINLINE_SIP_MY_IP="127.0.0.1", COINLINE_SIP_LOCAL_PORT=str(sip_bind), COINLINE_SIP_DTMF_MODE="auto")
    command = [sys.executable, str(ROOT / "tools/acceptance/graphic_session.py"), "--mame", str(args.mame.resolve()),
               "--firmware", str(args.firmware.resolve()), "--battery", str(args.battery.resolve()),
               "--settings", str(args.settings.resolve()), "--output", str(out / "terminal"),
               "--hardware", "multipay-v1.3", "--artwork", "--sound", "--quiet", "--dtmf-trace",
               "--snapshot-interval", "0", "--snapshot-native", "--external-co-port", jack, "--seconds", "300"]
    result = {"rtp_events": [], "passed": False}
    native = office = None
    run = None
    with (out / "co.log").open("wb") as co_log, (out / "launcher.log").open("wb") as launcher_log:
        try:
            office = subprocess.Popen([sys.executable, "-u", str(ROOT / "tools/line-jack/central_office.py")],
                                      env=env, cwd=out, stdout=co_log, stderr=subprocess.STDOUT)
            native = subprocess.Popen(command, env=env, cwd=ROOT, stdout=launcher_log, stderr=subprocess.STDOUT)

            def get_run():
                if native.poll() is not None:
                    raise RuntimeError(f"Terminal launcher exited {native.returncode}")
                runs = list((out / "terminal").glob("*/commands.txt"))
                return runs[0].parent if runs else None

            run = wait_for(get_run, 20, "terminal command file")
            commands = run / "commands.txt"
            commands.write_text(f"wait {args.startup_frames}\nsnapshot before-hook\nhook off\nwait 180\nkeyseq {dial}\n", encoding="ascii")
            peer.wait_request(b"INVITE", timeout=230)
            peer.wait_request(b"ACK", timeout=10)
            # The Windows native log is buffered until exit. Use the actual
            # SIP ACK and an emulated-time allowance, then verify that the
            # firmware enabled the keypad and drove each line event in the
            # final trace, rather than waiting for a buffered diagnostic.
            with commands.open("a", encoding="ascii") as target:
                target.write(f"wait {args.answer_frames}\nkeyseq 25*#\n")

            seen = {}
            expected = [2, 5, 10, 11]
            deadline = time.monotonic() + 20
            while time.monotonic() < deadline:
                try:
                    packet, _ = peer.rtp.recvfrom(8192)
                except socket.timeout:
                    continue
                if len(packet) != 16 or packet[1] & 0x7f != 101:
                    continue
                number, flag, duration = struct.unpack("!BBH", packet[12:])
                timestamp = int.from_bytes(packet[4:8], "big")
                event = seen.setdefault(timestamp, {"event": number, "marked_start": False, "end_packets": 0,
                                                    "duration_samples": 0})
                if event["event"] != number:
                    raise RuntimeError("An event changed digit without changing its RTP onset timestamp")
                event["marked_start"] |= bool(packet[1] & 0x80)
                event["duration_samples"] = max(event["duration_samples"], duration)
                event["end_packets"] += bool(flag & 0x80)
                if len(seen) == len(expected) and all(value["end_packets"] >= 3 for value in seen.values()):
                    break
            result["rtp_events"] = list(seen.values())
            if [event["event"] for event in seen.values()] != expected or not all(
                    event["marked_start"] and event["end_packets"] == 3 and event["duration_samples"] == 720
                    for event in seen.values()):
                raise RuntimeError("Connected native keypad did not produce the four expected RFC 4733 events")
            result["passed"] = True
        except Exception as error:
            result["error"] = str(error)
            raise
        finally:
            if native is not None and native.poll() is None:
                if run is not None:
                    with (run / "commands.txt").open("a", encoding="ascii") as target:
                        target.write("hook on\nwait 120\nexit\n")
                else:
                    native.terminate()
                try:
                    native.wait(timeout=20)
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
                result["passed"] = result["passed"] and native.returncode == 0
            if run is not None:
                log = read_log(run / "mame.log")
                native_digits = connected_digits(log)
                result["native_connected_digits"] = native_digits
                if result["passed"] and native_digits[-4:] != list("25*#"):
                    result["error"] = "Native hardware trace did not qualify the four transmitted digits"
                    result["passed"] = False
            (out / "result.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
            print(f"Native connected DTMF evidence: {out}", flush=True)
    if not result["passed"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
