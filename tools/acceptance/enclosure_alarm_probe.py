#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Drive the later enclosure alarm through genuine Z180 port writes and record native audio."""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path
import shutil
import socket
import statistics
import struct
import subprocess
import os
import time
import wave

from run import ROOT, port


def program(ring_overlap=False):
    code = bytearray(b"\xf3")  # DI
    if ring_overlap:
        # Enable the actual CO ringer, then wait for the line detector to
        # observe incoming ringing before driving the warning latch.
        code.extend(b"\x3e\x90\xed\x39\x40\xed\x38\x41\xe6\x80\x28\xf9")
    code.extend(b"\x01\xc3\x00")  # LD BC,00C3

    def delay():
        code.extend(b"\x21\xfa\x00\x2b\x7c\xb5\x20\xfb")  # 250-count HL loop, near 700 Hz at 12.288 MHz

    for base in (0x00, 0x0C, 0x04, 0x00):
        code.extend(b"\x11\x5e\x01")  # 350 waveform pairs
        at = len(code)
        for value in (base | 2, base):
            code.extend((0x3E, value, 0xED, 0x79))  # LD A, value; OUT (C),A
            delay()
        code.extend((0x1B, 0x7A, 0xB3, 0x20, (at - len(code) - 5) & 0xff))
    code.extend(b"\x76\x18\xfd")
    return code


def burst_program():
    """Synthetic 1 kHz short-warning profile, distinct from the reminder."""
    code = bytearray(b"\xf3\x01\xc3\x00")
    for base, pairs in ((0, 200),) + ((4, 30), (12, 40)) * 3:
        code.extend((0x11, pairs, 0))
        at = len(code)
        for value in (base | 2, base):
            code.extend((0x3e, value, 0xed, 0x79))
            code.extend(b"\x21\xae\x00\x2b\x7c\xb5\x20\xfb")
        code.extend((0x1b, 0x7a, 0xb3, 0x20, (at - len(code) - 5) & 0xff))
    code.extend(b"\x3e\x00\xed\x79\x76\x18\xfd")
    return code


def analyze_burst(out):
    rows = [json.loads(line) for line in (out / "gpio.jsonl").read_text().splitlines()]
    groups = []
    for row in rows:
        if row.get("port") != "0x00C3": continue
        base = int(row["value"], 16) & 12
        if not groups or groups[-1][0] != base: groups.append((base, []))
        groups[-1][1].append(row["timestamp_emulated_ns"] / 1e9)
    if [g[0] for g in groups] != [0, 4, 12, 4, 12, 4, 12, 0]:
        raise RuntimeError("Short warning did not alternate three times and restore the ring source")
    with wave.open(str(out / "enclosure.wav"), "rb") as wav:
        rate, channels = wav.getframerate(), wav.getnchannels()
        if wav.getsampwidth() != 2: raise RuntimeError("Expected native 16-bit output")
        data = list(struct.iter_unpack("<" + "h" * channels, wav.readframes(wav.getnframes())))
    result = []
    for index, (base, times) in enumerate(groups[1:-1], start=1):
        duration = groups[index + 1][1][0] - times[0]
        frequency = 1 / (2 * statistics.median(b - a for a, b in zip(times, times[1:])))
        segment = data[round((times[0] + .004) * rate):round((groups[index+1][1][0] - .004) * rate)]
        rms = max(math.sqrt(sum(v*v for v in channel) / len(channel)) for channel in zip(*segment))
        if abs(duration - (.030 if base == 4 else .040)) > .005 or not 900 <= frequency <= 1100:
            raise RuntimeError(f"Short warning timing outside synthetic fixture bound: {duration}, {frequency}")
        result.append({"selector_gate": base, "duration_ms": duration * 1000, "gpio_hz": frequency, "rms": rms})
    audible = min(r["rms"] for r in result if r["selector_gate"] == 4)
    if audible < 100 or any(r["rms"] > max(10, audible / 10) for r in result if r["selector_gate"] == 12):
        raise RuntimeError(f"Short warning audible/muted phases not isolated: {result}")
    (out / "result.json").write_text(json.dumps({"phases": result, "source_restored": True}, indent=2))
    print(json.dumps({"phases": result}, indent=2))


def analyze(out, ring_overlap=False):
    rows = [json.loads(line) for line in (out / "gpio.jsonl").read_text().splitlines()]
    rows = [row for row in rows if row.get("port") == "0x00C3"]
    groups = []
    for row in rows:
        base = int(row["value"], 16) & 0x0C
        if not groups or groups[-1][0] != base:
            groups.append((base, []))
        groups[-1][1].append(row["timestamp_emulated_ns"] / 1e9)
    if [base for base, _ in groups] != [0, 12, 4, 0] or any(len(times) != 700 for _, times in groups):
        raise RuntimeError(f"Unexpected native GPIO phases in {out}")
    with wave.open(str(out / "enclosure.wav"), "rb") as wav:
        rate, channels = wav.getframerate(), wav.getnchannels()
        if wav.getsampwidth() != 2:
            raise RuntimeError("Expected signed 16-bit native audio")
        data = list(struct.iter_unpack("<" + "h" * channels, wav.readframes(wav.getnframes())))
    metrics = []
    for base, times in groups:
        # Stay inside each stable phase, away from mixer and trace boundary quantization.
        segment = data[round((times[0] + .05) * rate):round((times[-1] - .05) * rate)]
        channel_metrics = []
        for channel, values in enumerate(zip(*segment)):
            rms = math.sqrt(sum(sample * sample for sample in values) / len(values))
            edges = sum((a >= 0) != (b >= 0) for a, b in zip(values, values[1:]))
            channel_metrics.append((rms, edges * rate / (2 * len(values)), channel))
        rms, measured, channel = max(channel_metrics)
        gpio_hz = 1 / (2 * statistics.median(b - a for a, b in zip(times, times[1:])))
        metrics.append({"selector_gate": base, "samples": len(segment), "rms": rms,
                        "gpio_hz": gpio_hz, "audio_hz": measured, "audio_channel": channel})
    audible = metrics[2]
    if audible["rms"] < 100 or abs(audible["audio_hz"] - audible["gpio_hz"]) > 15:
        raise RuntimeError(f"Native alarm waveform not matched: {metrics}")
    if ring_overlap:
        # The ring source must be audible before and after warning selection.
        # It must not leak into the muted warning phase, although the CO ring
        # input remains asserted throughout the test.
        if any(metrics[i]["rms"] < 100 or not 450 <= metrics[i]["audio_hz"] <= 680 for i in (0, 3)):
            raise RuntimeError(f"Incoming ring did not resume on source restoration: {metrics}")
        if metrics[1]["rms"] > max(10, audible["rms"] / 20):
            raise RuntimeError(f"Incoming ring leaked into muted warning phase: {metrics}")
    elif any(row["rms"] > max(10, audible["rms"] / 20) for i, row in enumerate(metrics) if i != 2):
        raise RuntimeError(f"Warning leaked into an unselected or muted phase: {metrics}")
    (out / "result.json").write_text(json.dumps({"phases": metrics, "ring_overlap": ring_overlap}, indent=2))
    print(json.dumps({"phases": metrics}, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mame", type=Path, required=True)
    parser.add_argument("--rompath", type=Path, default=ROOT / "build/package-inputs/local/roms")
    parser.add_argument("--controller", type=Path, default=ROOT / "firmware/tp_pcd3349a_behavioural.rom")
    parser.add_argument("--output", type=Path, default=ROOT / "build/firmware-acceptance/enclosure-alarm-probe")
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--short-warning", action="store_true", help="synthetic three-burst data-jack warning, not a paid-call test")
    mode.add_argument("--ring-overlap", action="store_true", help="hold a native line-jack ring input and verify warning-source arbitration")
    args = parser.parse_args()
    out = args.output.resolve() / datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    out.mkdir(parents=True)
    print(out, flush=True)
    shutil.copytree(ROOT / "fixtures", out / "fixtures")
    image = bytearray(b"\xff" * 1048576)
    code = burst_program() if args.short_warning else program(args.ring_overlap)
    image[0xc0000:0xc0000 + len(code)] = code
    seed = out / "flash.bin"
    seed.write_bytes(image)
    entry = {"version_id": "synthetic-enclosure-probe", "friendly_name": "Synthetic enclosure probe",
             "sha256_hex": hashlib.sha256(image).hexdigest(), "size_bytes": len(image), "motherboard": "later"}
    (out / "fixtures/firmware/firmware-hashes.json").write_text(json.dumps({"entries": [entry]}))
    env = {key: value for key, value in os.environ.items() if not key.upper().startswith("COINLINE_")}
    env.update(COINLINE_EMU_ROOT=str(out), COINLINE_MOTHERBOARD="later", COINLINE_HARDWARE_PRESET="multipay-v1.3",
               COINLINE_BOARD=str(ROOT / "fixtures/board/board-profile-2line-vfd.json"), COINLINE_FIRMWARE=str(seed),
               COINLINE_DIAGNOSTICS="0", COINLINE_TP8048_ROM=str(args.controller.resolve()),
               COINLINE_ALERTER_TRACE=str(out / "gpio.jsonl"))
    command = [str(args.mame.resolve()), "cl_millennium", "-noreadconfig", "-skip_gameinfo", "-nodebug",
               "-nolog", "-video", "none", "-sound", "auto", "-rompath", str(args.rompath.resolve()),
               "-nvram_directory", str(out / "nvram"), "-cfg_directory", str(out / "cfg"),
               "-seconds_to_run", "4", "-wavwrite", str(out / "enclosure.wav")]
    if args.ring_overlap:
        line_port = int(port())
        env["COINLINE_LINE_PORT"] = str(line_port)
        received = bytearray()
        with (out / "mame.log").open("wb") as log:
            process = subprocess.Popen(command, cwd=out, env=env, stdout=log, stderr=subprocess.STDOUT)
            try:
                deadline = time.monotonic() + 15
                while True:
                    try:
                        connection = socket.create_connection(("127.0.0.1", line_port), timeout=.2)
                        break
                    except OSError:
                        if process.poll() is not None or time.monotonic() >= deadline:
                            raise RuntimeError(f"Native line-jack did not listen: {out}")
                        time.sleep(.05)
                with connection:
                    connection.sendall(b"C1\nR1\n")
                    connection.settimeout(.2)
                    deadline = time.monotonic() + 20
                    while process.poll() is None and time.monotonic() < deadline:
                        try:
                            chunk = connection.recv(4096)
                            if not chunk:
                                break
                            if len(received) < 65536:
                                received.extend(chunk)
                        except socket.timeout:
                            pass
                    process.wait(timeout=5)
            finally:
                if process.poll() is None:
                    process.terminate()
                    process.wait(timeout=5)
        signals = received.decode("ascii", "replace").splitlines()
        (out / "line-signals.json").write_text(json.dumps([s for s in signals if not s.startswith("M")], indent=2))
        if process.returncode or "H0" not in signals or "E1" not in signals or "H1" in signals:
            raise RuntimeError(f"Native ring input or handset state not qualified: {out}")
    else:
        result = subprocess.run(command, cwd=out, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=90)
        (out / "mame.log").write_bytes(result.stdout)
        if result.returncode:
            raise RuntimeError(f"Native enclosure probe failed: {out}")
    if args.short_warning:
        analyze_burst(out)
    else:
        analyze(out, args.ring_overlap)


if __name__ == "__main__":
    main()
