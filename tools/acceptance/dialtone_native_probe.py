#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Record real firmware handset audio from pickup through the first digit."""

import argparse
from datetime import datetime, timezone
import json
import math
import os
from pathlib import Path
import shutil
import subprocess
import sys
import wave
from array import array

from run import ROOT, port


def meter(pcm, rate, frequency):
    omega = 2 * math.pi * frequency / rate
    coefficient = 2 * math.cos(omega)
    s1 = s2 = 0.0
    for value in pcm:
        s0 = value + coefficient * s1 - s2
        s2, s1 = s1, s0
    return 2 * math.sqrt(max(0.0, s1*s1 + s2*s2 - coefficient*s1*s2)) / len(pcm)


def measure(path, events):
    with wave.open(str(path), "rb") as wav:
        rate, channels = wav.getframerate(), wav.getnchannels()
        if wav.getsampwidth() != 2:
            raise ValueError("Expected signed 16-bit PCM")
        pcm = array("h", wav.readframes(wav.getnframes()))
    if sys.byteorder != "little":
        pcm.byteswap()
    pickup = next(int(row.split("\t")[0]) / 60 for row in events if row.endswith("\thook off"))
    digit = next(int(row.split("\t")[0]) / 60 for row in events if row.endswith("\tkeypad sequence"))
    hook_on = next(int(row.split("\t")[0]) / 60 for row in events if row.endswith("\thook on"))
    result = {"pickup_seconds": pickup, "digit_seconds": digit, "hangup_seconds": hook_on,
              "sample_rate": rate, "channels": channels, "observations": []}
    block = rate // 5
    for start in range(round(pickup * rate), round(hook_on * rate), block):
        segment = pcm[start*channels:(start + block)*channels]
        if len(segment) != block * channels:
            break
        # MAME may place its separate Voiceware and handset speakers on
        # different channels. Measure each before choosing tone evidence.
        rows = []
        for channel in range(channels):
            samples = segment[channel::channels]
            rms = math.sqrt(sum(v*v for v in samples) / block)
            low, high = (meter(samples, rate, f) for f in (350, 440))
            rows.append({"channel": channel+1, "rms": round(rms, 2),
                         "hz350": round(low, 2), "hz440": round(high, 2)})
        strongest = max(rows, key=lambda r: min(r["hz350"], r["hz440"]))
        result["observations"].append({"seconds": round(start/rate, 3), **strongest, "channel_metrics": rows})
    def dialtone(row):
        return row["rms"] >= 20 and min(row["hz350"], row["hz440"]) >= 0.4 * row["rms"]
    before = [r for r in result["observations"] if pickup + 0.4 <= r["seconds"] < digit - 0.2]
    after = [r for r in result["observations"] if digit + 0.6 <= r["seconds"] < hook_on - 0.2]
    result["dialtone_windows_before_digit"] = sum(dialtone(r) for r in before)
    result["dialtone_windows_after_digit"] = sum(dialtone(r) for r in after)
    result["passed"] = result["dialtone_windows_before_digit"] >= 5 and not result["dialtone_windows_after_digit"]
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package", type=Path, required=True)
    parser.add_argument("--profile", choices=("oem", "graphical"), required=True)
    parser.add_argument("--battery", type=Path, required=True)
    parser.add_argument("--mame", type=Path)
    parser.add_argument("--startup-frames", type=int, default=6000)
    parser.add_argument("--offhook-frames", type=int, default=180, help="wait before pressing the first digit")
    parser.add_argument("--dial", default="2", help="digits to enter in the isolated local office")
    parser.add_argument("--after-dial-frames", type=int, default=180)
    parser.add_argument("--require-dialtone", action="store_true", help="fail if the measured pre/post-digit gate fails")
    parser.add_argument("--require-native-waveform", action="store_true", help="require the native bank-0 dial-tone sample")
    parser.add_argument("--csio-trace", action="store_true", help="capture control-board serial edges for startup diagnosis")
    parser.add_argument("--output", type=Path, default=ROOT / "build/firmware-acceptance/dialtone")
    args = parser.parse_args()
    if args.startup_frames < 600:
        parser.error("Allow at least 600 startup frames")
    if not args.dial or any(digit not in "0123456789*#" for digit in args.dial) or args.after_dial_frames < 60:
        parser.error("Use keypad digits and at least 60 post-dial frames")
    package = args.package.resolve()
    out = args.output.resolve() / datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ") / args.profile
    out.mkdir(parents=True)
    shutil.copytree(args.battery, out / "nvram")
    graphic = args.profile == "graphical"
    board = "11line" if graphic else "2line"
    commands = out / "commands.txt"
    parts = ["wait 300", "snapshot boot-5s", "wait 300", "snapshot boot-10s"]
    if args.startup_frames > 600:
        parts += [f"wait {args.startup_frames - 600}"]
    parts += ["snapshot before-hook", "hook off", f"wait {args.offhook_frames}", "snapshot before-digit",
              f"keyseq {args.dial}", f"wait {args.after_dial_frames}", "snapshot after-digit", "hook on", "wait 60", "exit"]
    commands.write_text("\n".join(parts) + "\n", encoding="ascii")
    env = {key: value for key, value in os.environ.items() if not key.upper().startswith("COINLINE_")}
    env.update(COINLINE_EMU_ROOT=str(package), COINLINE_MOTHERBOARD="later" if graphic else "earlier",
               COINLINE_HARDWARE_PRESET="multipay-v1.3" if graphic else "multipay-v1",
               COINLINE_CARD_DMA_CHANNEL="1" if graphic else "0", COINLINE_DIAGNOSTICS="0",
               COINLINE_BOARD=str(package / f"fixtures/board/board-profile-{board}-vfd.json"),
               COINLINE_TP_BACKEND="pcd3349a", COINLINE_TP8048_ROM=str(package / "firmware/tp_pcd3349a_behavioural.rom"),
               COINLINE_FIRMWARE=str(package / f"firmware/{args.profile}/flash.bin"),
               COINLINE_LINE_PORT=port(), COINLINE_NCC_PORT=port(), COINLINE_CO_MODE="local", COINLINE_CO_TIMEOUT="0",
               COINLINE_GRAPHIC_COMMANDS=str(commands), COINLINE_GRAPHIC_EVENTS=str(out / "events.tsv"),
               COINLINE_GRAPHIC_SNAPSHOT_INTERVAL="0", COINLINE_HW_CNTL_RELAY_TRACE=str(out / "relays.jsonl"),
               COINLINE_TP_8048_CP_PROTOCOL_TRACE=str(out / "telephony.jsonl"),
               COINLINE_TP_RUNTIME_HEALTH_TRACE=str(out / "telephony-health.jsonl"),
               COINLINE_DTMF_TRACE="1",
               COINLINE_MIC_INPUT="_disabled_in_isolated_waveform_probe_",
               COINLINE_VOICEWARE_TRACE=str(out / "voiceware.jsonl"))
    if not graphic:
        env["COINLINE_VFD_LIVE"] = str(out / "display.txt")
    if args.csio_trace:
        env.update(COINLINE_TP_CSIO_RAW_TRACE=str(out / "csio.jsonl"),
                   COINLINE_TP_CSIO_QUALIFIED_TRACE=str(out / "csio-qualified.jsonl"))
    executable = args.mame.resolve() if args.mame else package / "bin/coinline-mame.exe"
    command = [str(executable), "cl_millennium", "-noreadconfig", "-skip_gameinfo", "-nodebug", "-nolog",
               "-video", "none", "-sound", "none", "-rompath", str(package / "roms"),
               "-artpath", str(package / "artwork"), "-nvram_directory", str(out / "nvram"),
               "-cfg_directory", str(out / "cfg"), "-snapshot_directory", str(out), "-snapview", "native",
               "-wavwrite", str(out / "handset.wav"), "-seconds_to_run",
               str((args.startup_frames + args.offhook_frames + args.after_dial_frames) // 60 + 30),
               "-autoboot_delay", "0", "-autoboot_script", str(ROOT / "tools/acceptance/graphic_session.lua")]
    print(out, flush=True)
    with (out / "co.log").open("wb") as co_log, (out / "mame.log").open("wb") as native_log:
        office = subprocess.Popen([sys.executable, "-u", str(ROOT / "tools/line-jack/central_office.py")],
                                  cwd=out, env=env, stdout=co_log, stderr=subprocess.STDOUT)
        try:
            native = subprocess.run(command, cwd=out, env=env, stdout=native_log, stderr=subprocess.STDOUT,
                                    timeout=300)
        finally:
            try:
                office.wait(timeout=5)
            except subprocess.TimeoutExpired:
                office.terminate()
                office.wait(timeout=5)
    if native.returncode:
        raise RuntimeError(f"Native emulator exit {native.returncode}: {out}")
    result = measure(out / "handset.wav", (out / "events.tsv").read_text().splitlines())
    health = [json.loads(line) for line in (out / "telephony-health.jsonl").read_text().splitlines()]
    result["boot_handshake_passed"] = any(row.get("telephony_response") == "0xC0_len8"
        and row.get("frame_checksum_ok") and row.get("ready_state") for row in health)
    result["passed"] = result["passed"] and result["boot_handshake_passed"]
    voice = [json.loads(line) for line in (out / "voiceware.jsonl").read_text().splitlines()]
    starts = [e for e in voice if e.get("event_type") == "voice_segment_start" and e.get("playback_started")]
    result["native_dialtone"] = any(e.get("active_bank") == 0 and e.get("raw_sample_index") == "0x12"
                                    and e.get("execution_path") == "upd7759_waveform" for e in starts)
    result["waveform_starts"] = [e["raw_sample_index"] for e in starts if e.get("execution_path") == "upd7759_waveform"]
    result["adpcm_starts"] = sum(e.get("execution_path") == "upd7759_core" for e in starts)
    if args.require_native_waveform:
        result["passed"] = result["passed"] and result["native_dialtone"]
    (out / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(f"Captured pickup, dial and hangup in {out}")
    print({key: result[key] for key in ("dialtone_windows_before_digit", "dialtone_windows_after_digit", "boot_handshake_passed", "passed")})
    return 1 if (args.require_dialtone or args.require_native_waveform) and not result["passed"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
