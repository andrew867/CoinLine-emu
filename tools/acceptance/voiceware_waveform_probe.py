#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Exercise native uPD7759 waveform records over the terminal's actual I/O bus.

Requires user-supplied Voiceware ROMs. Only aggregate timings and spectral
measurements are reported; neither ROM bytes nor recordings are committed.
"""
import argparse
from array import array
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import subprocess
import sys
import wave

from run import ROOT, port
from dialtone_native_probe import meter


def records(bank):
    offsets = [int.from_bytes(bank[5+2*i:7+2*i], "big") * 2 for i in range(bank[0]+1)]
    for sample, start in enumerate(offsets):
        if bank[start] != 0x40:
            continue
        limit = min((p for p in offsets if p > start), default=len(bank))
        p, divider, clocks, periods = start + 1, 0, 0, []
        while p < limit:
            op = bank[p]
            p += 1
            if op == 0:
                break
            if op >= 0xc0:
                parameter = bank[p]
                p += 1
                divider = 4 * ((parameter & 63) + 1) + (parameter >> 6)
            elif op == 0x2f:
                clocks += 48 * 1024
            elif 0x40 <= op < 0x80 or 1 <= op <= 7:
                length = (12,16,8,48,96,32,24,64)[op & 7]
                repeat = bank[p] + 1
                assert divider and p + 1 + length <= limit
                duration = length * repeat * divider
                periods.append([clocks / 640000, duration / 640000])
                clocks += duration
                p += 1 + length
            else:
                raise ValueError(f"Unknown waveform command in sample {sample:02x}")
        else:
            raise ValueError(f"Unterminated waveform sample {sample:02x}")
        yield sample, clocks / 640000, periods


LUA = r"""
local machine = manager.machine
local bus = machine.devices[":maincpu"].spaces["io"]
local records = RECORDS
local frame, index, phase, started, seen_busy = 0, 1, 0, 0, false
local output = assert(io.open("bus-results.tsv", "w"))
local function report(tag, sample)
    output:write(string.format("%s\t%d\t%d\n", tag, sample, frame)); output:flush()
end
emu.register_frame_done(function()
    frame = frame + 1
    if frame < 10 then return end
    local current = records[index]
    if not current then
        output:close()
        print("VOICEWARE_WAVEFORM_PROBE passed")
        machine:exit()
        return
    end
    local sample, cutoff = current[1], current[2]
    if phase == 0 then
        bus:write_u8(0x40, 0x00)
        bus:write_u8(0x42, 0x00)
        phase = 1
    elseif phase == 1 then
        bus:write_u8(0x40, 0x08)
        phase = 2
    elseif phase == 2 then
        assert(bus:read_u8(0x61) == 0xff, "not idle before new sample")
        bus:write_u8(0x61, sample)
        report("start", sample)
        started, seen_busy, phase = frame, false, 3
    elseif phase == 3 then
        local status = bus:read_u8(0x61)
        if status == 0x7f then
            if not seen_busy then report("busy", sample) end
            seen_busy = true
        end
        if cutoff > 0 and frame - started >= cutoff then
            assert(status == 0x7f, "reset test sample ended prematurely")
            bus:write_u8(0x40, 0x00)
            report("reset", sample)
            phase = 4
        elseif seen_busy and status == 0xff then
            report("complete", sample)
            phase, index = 0, index + 1
        elseif frame - started > 1800 then
            error("BUSY did not complete for sample " .. sample)
        end
    elseif phase == 4 then
        bus:write_u8(0x40, 0x08)
        phase = 5
    elseif phase == 5 then
        assert(bus:read_u8(0x61) == 0xff, "reset did not clear playback")
        report("reset-idle", sample)
        phase, index = 0, index + 1
    end
end, "native waveform elements")
"""


def analyze_audio(path, records_by_sample, starts):
    with wave.open(str(path), "rb") as wav:
        rate, channels = wav.getframerate(), wav.getnchannels()
        assert wav.getsampwidth() == 2
        pcm = array("h", wav.readframes(wav.getnframes()))
    if sys.byteorder != "little":
        pcm.byteswap()

    def tone(sample, begin, end, frequencies):
        start = starts[sample] + begin
        stop = starts[sample] + end
        block = pcm[round(start*rate)*channels:round(stop*rate)*channels]
        rows = []
        for channel in range(channels):
            frames = block[channel::channels]
            if not frames:
                raise ValueError("Missing waveform audio interval")
            rms = math.sqrt(sum(x*x for x in frames) / len(frames))
            amplitudes = [meter(frames, rate, f) for f in frequencies]
            rows.append((rms, amplitudes))
        rms, amplitudes = max(rows, key=lambda row: max(row[1]))
        return rms, amplitudes

    # Begin well inside an element to avoid scheduling/analogue filter edges.
    attention = []
    for (offset, duration), frequency in zip(records_by_sample[0x14][1], (914.3,1367.5,1818.2)):
        rms, values = tone(0x14, offset+0.025, offset+duration-0.025, [frequency])
        assert rms > 10 and values[0] > rms * 0.8, (frequency, rms, values)
        attention.append({"hz": frequency, "rms": round(rms, 2), "amplitude": round(values[0],2)})
    dial_rms, dial_values = tone(0x12, .1, .7, [350.9,438.6])
    assert dial_rms > 10 and min(dial_values) > dial_rms * .35, (dial_rms, dial_values)
    levels = {}
    for sample, (_, periods) in records_by_sample.items():
        offset, length = periods[0]
        rms, _ = tone(sample, offset+length*.25, offset+length*.75, [1000])
        assert rms > 3, (sample, rms)
        levels[f"{sample:02x}"] = round(rms, 2)
    bong_duration = records_by_sample[0x13][0]
    # The inferred changing gain is audible as a decay; absolute analogue
    # gain and its transfer law are not a physical measurement.
    bong_early, _ = tone(0x13,.025,.09,[1000])
    bong_late, _ = tone(0x13,bong_duration-.1,bong_duration-.025,[1000])
    assert bong_early > bong_late > 0, (bong_early,bong_late)
    dtmf = []
    standards = [697,770,852,941,1209,1336,1477,1633]
    for sample in range(0x18, 0x28):
        length = records_by_sample[sample][1][0][1]
        rms, values = tone(sample, .012, length-.01, standards)
        row = max(range(4), key=lambda i: values[i])
        col = max(range(4,8), key=lambda i: values[i])
        assert rms > 10 and min(values[row],values[col]) > rms * .35, (sample,rms,values)
        dtmf.append([standards[row],standards[col]])
    assert len(set(map(tuple,dtmf))) == 16, dtmf
    return {"sample_rate": rate, "channels": channels, "attention": attention,
            "dialtone": {"rms": round(dial_rms,2), "amplitudes": [round(v,2) for v in dial_values],
                         "frequencies_hz": [350.9,438.6]}, "dtmf_pairs_hz": dtmf,
            "bong_rms_early_late": [round(bong_early,2),round(bong_late,2)], "waveform_rms":levels}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mame", type=Path, required=True)
    parser.add_argument("--rompath", type=Path, required=True)
    parser.add_argument("--controller", type=Path, default=ROOT / "firmware/tp_pcd3349a_behavioural.rom")
    parser.add_argument("--output", type=Path, default=ROOT / "build/firmware-acceptance/voiceware-waveform")
    args = parser.parse_args()
    out = args.output.resolve() / datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    out.mkdir(parents=True)
    print(out, flush=True)
    shutil.copytree(ROOT / "fixtures", out / "fixtures")
    image = bytearray(b"\xff" * 1048576)
    image[0xc0000:0xc0004] = b"\xf3\x76\x18\xfd"  # DI, HALT, JR back
    seed = out / "flash.bin"
    seed.write_bytes(image)
    entry = {"version_id":"synthetic-voiceware-waveform", "friendly_name":"Synthetic waveform probe",
             "sha256_hex":hashlib.sha256(image).hexdigest(), "size_bytes":len(image), "motherboard":"later"}
    (out / "fixtures/firmware/firmware-hashes.json").write_text(json.dumps({"entries":[entry]}))
    bank = (args.rompath / "cl_millennium/voice_u16.bin").read_bytes()[:0x20000]
    inventory = {sample:(duration, periods) for sample,duration,periods in records(bank)}
    assert all(sample in inventory for sample in (0x12,0x13,0x14,0x31,*range(0x18,0x28)))
    # Full records first, then abort a separately started long dial tone.
    playlist = list(inventory) + [0x12]
    script = out / "probe.lua"
    script.write_text(LUA.replace("RECORDS", "{" + ",".join(
        "{"+str(s)+","+str(8 if i == len(playlist)-1 else 0)+"}" for i,s in enumerate(playlist)) + "}"))
    env = {key:value for key,value in os.environ.items() if not key.upper().startswith("COINLINE_")}
    env.update(COINLINE_EMU_ROOT=str(out), COINLINE_MOTHERBOARD="later", COINLINE_HARDWARE_PRESET="multipay-v1.3",
               COINLINE_BOARD=str(ROOT / "fixtures/board/board-profile-2line-vfd.json"), COINLINE_FIRMWARE=str(seed),
               COINLINE_DIAGNOSTICS="0", COINLINE_TP8048_ROM=str(args.controller.resolve()),
               COINLINE_LINE_PORT=str(port()), COINLINE_NCC_PORT=str(port()),
               COINLINE_MIC_INPUT="_disabled_in_isolated_waveform_probe_", COINLINE_VOICEWARE_TRACE=str(out / "voice.jsonl"))
    wav = out / "native-playback.wav"
    command = [str(args.mame.resolve()),"cl_millennium","-noreadconfig","-skip_gameinfo","-nodebug","-nolog",
               "-video","none","-sound","none","-nothrottle","-rompath",str(args.rompath.resolve()),
               "-nvram_directory",str(out/"nvram"),"-cfg_directory",str(out/"cfg"),"-wavwrite",str(wav),
               "-seconds_to_run",str(math.ceil(sum(v[0] for v in inventory.values()))+15),
               "-autoboot_delay","0","-autoboot_script",str(script)]
    with (out / "mame.log").open("wb") as log:
        subprocess.run(command,cwd=out,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=300,check=True)
    assert "VOICEWARE_WAVEFORM_PROBE passed" in (out / "mame.log").read_text(errors="replace")
    bus_rows = [(tag,int(sample),int(frame)) for tag,sample,frame in
                (line.split("\t") for line in (out/"bus-results.tsv").read_text().splitlines())]
    completed, starts = {}, {}
    for tag,sample,frame in bus_rows:
        if tag == "start" and sample not in starts:
            starts[sample] = frame / 60
        if tag == "complete":
            completed[sample] = frame / 60 - starts[sample]
    assert completed.keys() == inventory.keys()
    assert all(abs(duration-inventory[sample][0]) < .05 for sample,duration in completed.items())
    trace = [json.loads(line) for line in (out/"voice.jsonl").read_text().splitlines()]
    waveform_starts = [e for e in trace if e.get("event_type") == "voice_segment_start"]
    assert len(waveform_starts) == len(playlist) and all(e.get("execution_path") == "upd7759_waveform" for e in waveform_starts)
    pending, interrupt_durations = None, {}
    for event in trace:
        if event.get("event_type") == "voice_segment_start":
            pending = event
        elif event.get("event_type") == "voice_irq0_assert":
            assert pending is not None
            sample = int(pending["raw_sample_index"], 16)
            assert sample not in interrupt_durations
            # This probe runs the later motherboard at 12.288 MHz.
            seconds = (event["cycle"] - pending["cycle"]) / 12288000
            assert abs(seconds - inventory[sample][0]) < .001, (sample,seconds)
            interrupt_durations[sample] = seconds
            pending = None
    assert interrupt_durations.keys() == inventory.keys()
    # Use traced emulated timestamps rather than rounded video-frame timestamps for audio.
    for e in waveform_starts[:-1]:
        starts[int(e["raw_sample_index"],16)] = e["timestamp_emulated_ns"]/1e9
    result = {"passed": True, "waveform_samples":len(inventory), "completion_interrupts":len(inventory),
              "reset_abort_passed": True, "durations_seconds":{f"{s:02x}":round(d,4) for s,d in completed.items()},
              "completion_irq_ms":{f"{s:02x}":round(d*1000,4) for s,d in interrupt_durations.items()},
              "audio":analyze_audio(wav,inventory,starts)}
    (out/"result.json").write_text(json.dumps(result,indent=2)+"\n")
    print(f"Native waveform probe: {len(inventory)} records, BUSY/INT0 and PCM passed",flush=True)


if __name__ == "__main__":
    main()
