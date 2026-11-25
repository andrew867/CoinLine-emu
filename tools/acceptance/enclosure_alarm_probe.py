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
import statistics
import struct
import subprocess
import os
import wave

from run import ROOT


def program():
    code = bytearray(b"\xf3\x01\xc3\x00")  # DI; LD BC,00C3

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


def analyze(out):
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
    if any(row["rms"] > max(10, audible["rms"] / 20) for i, row in enumerate(metrics) if i != 2):
        raise RuntimeError(f"Warning leaked into an unselected or muted phase: {metrics}")
    (out / "result.json").write_text(json.dumps({"phases": metrics}, indent=2))
    print(json.dumps({"phases": metrics}, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mame", type=Path, required=True)
    parser.add_argument("--rompath", type=Path, default=ROOT / "build/package-inputs/local/roms")
    parser.add_argument("--controller", type=Path, default=ROOT / "firmware/tp_pcd3349a_behavioural.rom")
    parser.add_argument("--output", type=Path, default=ROOT / "build/firmware-acceptance/enclosure-alarm-probe")
    args = parser.parse_args()
    out = args.output.resolve() / datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    out.mkdir(parents=True)
    print(out, flush=True)
    shutil.copytree(ROOT / "fixtures", out / "fixtures")
    image = bytearray(b"\xff" * 1048576)
    code = program()
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
    result = subprocess.run(command, cwd=out, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=90)
    (out / "mame.log").write_bytes(result.stdout)
    if result.returncode:
        raise RuntimeError(f"Native enclosure probe failed: {out}")
    analyze(out)


if __name__ == "__main__":
    main()
