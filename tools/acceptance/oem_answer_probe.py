#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Cold OEM INSTALL through answer supervision, stopping before NCC download."""

import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import subprocess
import sys

from run import ROOT, port

sys.path.insert(0, str(ROOT / "tools/line-jack"))
from test_connected_dtmf_sip_loopback import LoopbackPeer


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package", type=Path, required=True, help="extracted Windows OEM package with local ROM inputs")
    parser.add_argument("--mame", type=Path, help="override packaged native executable")
    parser.add_argument("--mode", choices=("local", "sip"), default="sip")
    parser.add_argument("--office", type=Path, default=ROOT / "tools/line-jack/central_office.py")
    parser.add_argument("--output", type=Path, default=ROOT / "build/firmware-acceptance/oem-answer")
    args = parser.parse_args()
    package = args.package.resolve()
    out = args.output.resolve() / datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    out.mkdir(parents=True)
    for directory in ("nvram", "cfg"):
        (out / directory).mkdir()
    display = out / "display.txt"
    co_log = out / "co.log"
    env = {key: value for key, value in os.environ.items() if not key.upper().startswith("COINLINE_")}
    env.update(COINLINE_EMU_ROOT=str(package), COINLINE_MOTHERBOARD="earlier", COINLINE_DIAGNOSTICS="0",
               COINLINE_HARDWARE_PRESET="multipay-v1", COINLINE_CARD_DMA_CHANNEL="0",
               COINLINE_BOARD=str(package / "fixtures/board/board-profile-2line-vfd.json"),
               COINLINE_TP_BACKEND="pcd3349a", COINLINE_TP8048_ROM=str(package / "firmware/tp_pcd3349a_behavioural.rom"),
               COINLINE_FIRMWARE=str(package / "firmware/oem/flash.bin"), COINLINE_LINE_PORT=port(),
               COINLINE_NCC_PORT=port(), COINLINE_CO_MODE=args.mode, COINLINE_CO_TIMEOUT="0",
               COINLINE_VFD_LIVE=str(display), COINLINE_ACCEPTANCE_CO_LOG=str(co_log),
               COINLINE_ACCEPTANCE_LINE="2025550123", COINLINE_ACCEPTANCE_SERIAL="0102030405",
               COINLINE_ACCEPTANCE_PIN="55555", COINLINE_ACCEPTANCE_KEYS="+-LN1234567890*#",
               COINLINE_ACCEPTANCE_ANSWER_ONLY="1", COINLINE_MODEM_TRACE="1")
    peer = None
    if args.mode == "sip":
        import socket
        peer = LoopbackPeer()
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
            probe.bind(("127.0.0.1", 0))
            bind_port = probe.getsockname()[1]
        env.update(COINLINE_SIP_HOST="127.0.0.1", COINLINE_SIP_PORT=str(peer.port), COINLINE_SIP_USER="lab",
                   COINLINE_SIP_PASSWORD="loopback-fixture", COINLINE_SIP_MY_IP="127.0.0.1",
                   COINLINE_SIP_LOCAL_PORT=str(bind_port))
    command = [str(args.mame.resolve() if args.mame else package / "bin/coinline-mame.exe"), "cl_millennium", "-noreadconfig", "-skip_gameinfo",
               "-nodebug", "-nolog", "-rompath", str(package / "roms"), "-artpath", str(package / "artwork"),
               "-nvram_directory", str(out / "nvram"), "-cfg_directory", str(out / "cfg"), "-video", "none",
               "-sound", "none", "-seconds_to_run", "240", "-autoboot_delay", "0", "-autoboot_script",
               str(ROOT / "tools/acceptance/install.lua")]
    native_code = None
    try:
        with co_log.open("wb") as office_log, (out / "mame.log").open("wb") as native_log:
            office = subprocess.Popen([sys.executable, "-u", str(args.office.resolve())], cwd=out, env=env,
                                      stdout=office_log, stderr=subprocess.STDOUT)
            try:
                native_code = subprocess.run(command, cwd=out, env=env, stdout=native_log,
                                             stderr=subprocess.STDOUT, timeout=300).returncode
            finally:
                try:
                    office.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    office.terminate()
                    office.wait(timeout=5)
    finally:
        if peer is not None:
            peer.close()
    history = Path(str(display) + ".events").read_text(encoding="utf-8", errors="replace")
    log = co_log.read_text(encoding="utf-8", errors="replace")
    checks = {
        "co_completed": "observed\tco result completed 00" in history,
        "answer_completed": "observed\tanswer result completed 00" in history,
        "download_not_started": "passed\tdownload: answer supervision completed; NCC download not started" in history,
        "local_answer_reversals": log.count("manager answer supervision"),
        "external_invite": "sip sending outbound INVITE" in log,
        "native_exit_code": native_code,
    }
    checks["passed"] = (checks["co_completed"] and checks["answer_completed"] and checks["download_not_started"]
                        and checks["local_answer_reversals"] == 1 and not checks["external_invite"] and native_code == 0)
    (out / "result.json").write_text(json.dumps(checks, indent=2) + "\n", encoding="utf-8")
    print(f"OEM answer detection evidence: {out}")
    print(json.dumps(checks, indent=2))
    return 0 if checks["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
