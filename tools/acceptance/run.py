#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Run firmware INSTALL using the real MAME input ports and a loopback NCC."""

import argparse
from contextlib import contextmanager
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import socket
import subprocess
import sys
import time


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/windows/portable"))
from hardware import PRESETS, program_tag, select_hardware

PRIORITY = ("NBA1F02", "NKA1X02", "NBJ1S03", "NPE1S01", "NCA1X03", "NQA1X01")
SINGLE_PROGRAM_ROLES = ("program", "modified program", "demo program")
SIGNALS = {
    "boot": "cold boot display", "craft": "Craft accepted access input",
    "co_line": "co result completed 00", "answer_supervision": "answer result completed 00",
    "keypad": "keypad check advanced", "card": "card check advanced",
    "smartcard": "smart card check advanced", "smartcard_failed": "smart card check failed",
    "card_failed": "card check failed",
    "coin": "coin check completed 00", "install": "INSTALL complete",
    "restart": "in service after restart", "local_call": "local CO answered",
}


def sha256(path):
    with path.open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def flash_evidence(path, expected_seed):
    if not path.is_file():
        return None
    raw = path.read_bytes()
    if len(raw) != 64 + 1048576 or raw[:64] != expected_seed.encode("ascii"):
        raise ValueError("Persistent flash has the wrong size or initial image digest")
    data = raw[64:]
    return {"seed_sha256": expected_seed,
            "active_sha256": hashlib.sha256(data).hexdigest(),
            "chip_sha256": [hashlib.sha256(data[index:index + 524288]).hexdigest()
                            for index in (0, 524288)]}


def json_file(path, value):
    temp = path.with_suffix(path.suffix + ".tmp")
    temp.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")
    # Windows denies replacement while a report reader holds the old file open.
    deadline = time.monotonic() + 3
    while True:
        try:
            temp.replace(path)
            return
        except PermissionError as error:
            if getattr(error, "winerror", None) not in (5, 32, 33) or time.monotonic() >= deadline:
                raise
            time.sleep(0.05)


def read(path):
    return path.read_text(encoding="utf-8", errors="replace") if path.is_file() else ""


def inputs(library, inventory):
    verified = {}
    for entry in inventory["files"]:
        name = entry["path"]
        path = (library / name).resolve()
        if not path.is_relative_to(library) or name in verified:
            raise ValueError(f"Unexpected inventory path: {name}")
        digest = sha256(path) if path.is_file() else None
        ok = digest == entry["sha256"] and path.stat().st_size == entry["bytes"]
        verified[name] = {**entry, "actual_sha256": digest, "verified": ok}
    return verified


def candidates(row, verified):
    by_role = {}
    for name in row["images"]:
        item = verified[name]
        by_role.setdefault(item["role"], {}).setdefault(item["sha256"], item)
    for role in SINGLE_PROGRAM_ROLES:
        for item in by_role.get(role, {}).values():
            if item["bytes"] == 262144:
                yield [item], "earlier"
    for first, second in (("flash U2", "flash X1"), ("flash 0", "flash 1")):
        a, b = list(by_role.get(first, {}).values()), list(by_role.get(second, {}).values())
        if len(a) == len(b) == 1 and a[0]["bytes"] == b[0]["bytes"] == 524288:
            yield a + b, "later"


def voice_pair(row, verified, reference):
    pair = []
    for role, filename in (("voiceware U16", "voice_u16.bin"), ("voiceware U26", "voice_u26.bin")):
        matches = [verified[name] for name in row["images"] if verified[name]["role"] == role]
        if len({item["sha256"] for item in matches}) != 1 or not all(
            item["verified"] and item["bytes"] == 1048576 for item in matches
        ):
            return "shared lab ROMs", [reference / "roms/cl_millennium" / name
                                       for name in ("voice_u16.bin", "voice_u26.bin")]
        pair.append(Path(matches[0]["path"]))
    return "archive pair", pair


def frame_crc(data):
    crc = 0
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ (0xA001 if crc & 1 else 0)
    return crc


def ncc_frames(log):
    for session, trace in enumerate(log.split("modem carrier (SSI) dcd=1")[1:], 1):
        for direction in ("tx", "rx"):
            stream = bytes(int(value, 16) for value in re.findall(
                rf"^modem {direction} 0x([0-9a-f]{{2}})", trace, re.M | re.I))
            pos = 0
            while pos + 3 < len(stream):
                if stream[pos] != 2:
                    pos += 1
                    continue
                size = stream[pos + 2] + 1
                frame = stream[pos:pos + size]
                if (size < 6 or len(frame) != size or frame[-1] != 3 or
                    frame_crc(frame[:-3]) != int.from_bytes(frame[-3:-1], "little")):
                    pos += 1
                    continue
                payload = frame[8:-3]
                result = {"session": session, "direction": direction, "control": frame[1], "wire_bytes": size}
                if payload:
                    result["leading_type"] = payload[0]
                    result["payload_sha256"] = hashlib.sha256(payload).hexdigest()
                    if direction == "tx" and payload[0] == 14 and len(payload) == 2:
                        result["table_ack"] = payload[1]
                    if direction == "tx" and payload[0] == 44 and len(payload) >= 2:
                        result["table_request_reason"] = payload[1]
                yield result
                pos += size


def events(path):
    for line in read(path).splitlines():
        parts = line.split("\t", 2)
        if len(parts) == 3 and parts[0].isdigit():
            yield {"frame": int(parts[0]), "event": parts[1], "detail": parts[2]}


def port():
    with socket.socket() as server:
        server.bind(("127.0.0.1", 0))
        return str(server.getsockname()[1])


@contextmanager
def central_office(run, env, python):
    with (run / "co.log").open("wb") as log:
        process = subprocess.Popen([str(python), "-u", str(ROOT / "tools/line-jack/central_office.py")],
                                   stdout=log, stderr=subprocess.STDOUT, cwd=run, env=env)
        try:
            yield
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()


def wait_for_mame(process, display, startup_timeout, timeout, require_display=True):
    start = time.monotonic()
    try:
        return process.wait(timeout=min(startup_timeout, timeout)), None
    except subprocess.TimeoutExpired:
        if require_display and not re.search(r"[^\s*|]", read(display)):
            return None, "no startup display before the wall-clock deadline"
    try:
        return process.wait(timeout=max(0, timeout - (time.monotonic() - start))), None
    except subprocess.TimeoutExpired:
        return None, "wall timeout"


def run_phase(args, settings, attempt, name, base_env):
    run = attempt / name
    run.mkdir()
    cfg = run / "cfg"
    cfg.mkdir()
    display = run / "display.txt"
    env = dict(base_env, COINLINE_LINE_PORT=port(), COINLINE_VFD_LIVE=str(display),
               COINLINE_ACCEPTANCE_CO_LOG=str(run / "co.log"),
               COINLINE_ACCEPTANCE_WARM="1" if name == "warm" else "0",
               COINLINE_ACCEPTANCE_PORTABLE_STAGE="prepare" if name == "portable-preflight" else
               "attached" if name == "cold" and base_env.get("COINLINE_ACCEPTANCE_COIN_BASIC") == "1" else "",
               COINLINE_ACCEPTANCE_PROBE="1" if args.boot_only else "0",
               COINLINE_BOOT_TRACE=str(run / "boot.jsonl"),
               COINLINE_TRACE_ONLY="acceptance-trace")
    detached = name in ("portable-preflight", "detached")
    env["COINLINE_PORTABLE_VFD"] = "0" if detached else "1"
    env["COINLINE_ACCEPTANCE_HEADLESS"] = "1" if name == "detached" else "0"
    if base_env.get("COINLINE_ACCEPTANCE_COIN_BASIC") == "1" and name != "detached":
        env["COINLINE_ACCEPTANCE_DIAL"] = ""
    if "desk_key_card" in settings and name == "cold":
        card = run / "card.json"
        shutil.copyfile(base_env["COINLINE_MAG_CARD"], card)
        env["COINLINE_MAG_CARD"] = str(card)
        env["COINLINE_ACCEPTANCE_DESK_CARD"] = str(Path(settings["desk_key_card"]).resolve())
        env["COINLINE_ACCEPTANCE_TEST_CARD"] = base_env["COINLINE_MAG_CARD"]
    if args.hardware_trace:
        env.update(COINLINE_TRACE_INTERRUPTS="1", COINLINE_INTERRUPT_TRACE=str(run / "acceptance-trace-interrupts.jsonl"),
                   COINLINE_TRACE_MMU_TRANSLATION="1", COINLINE_MMU_TRANSLATION_TRACE=str(run / "acceptance-trace-mmu.jsonl"),
                   COINLINE_Z180_REG_TRACE=str(run / "acceptance-trace-z180.jsonl"))
    if args.io_trace:
        env["COINLINE_IO_TRACE"] = str(run / "acceptance-trace-io.jsonl")
    if args.uart_trace:
        env.update(COINLINE_TRACE_ASCI="1",
                   COINLINE_EXTERNAL_UART_TRACE=str(run / "acceptance-trace-external-uart.jsonl"))
    if args.eeprom_trace:
        env.update(COINLINE_TRACE_MICROWIRE="1",
                   COINLINE_MICROWIRE_TRACE=str(run / "acceptance-trace-microwire.jsonl"))
    if args.read_watch and name == "cold":
        env.update(COINLINE_ACCEPTANCE_READ_WATCH=str(args.read_watch.resolve()),
                   COINLINE_ACCEPTANCE_READ_WATCH_OUT=str(run / "memory-reads.tsv"))
    if args.telephony_trace:
        env.update(COINLINE_TRACE_ASCI="1",
                   COINLINE_TP_CSIO_RAW_TRACE=str(run / "acceptance-trace-csio-wire.jsonl"),
                   COINLINE_TP_CSIO_QUALIFIED_TRACE=str(run / "acceptance-trace-csio.jsonl"),
                   COINLINE_TP_8048_CP_PROTOCOL_TRACE=str(run / "acceptance-trace-telephony.jsonl"))
    if args.voice_trace or ((args.test_call or args.test_mag_call) and name == "warm"):
        env["COINLINE_VOICEWARE_TRACE"] = str(run / "acceptance-trace-voiceware.jsonl")
    if args.card_trace:
        env["COINLINE_SMART_CARD_TRACE"] = "1"
    command = [str(args.mame), "cl_millennium", "-noreadconfig", "-skip_gameinfo", "-nodebug", "-nolog",
               "-video", "none", "-sound", "none", "-rompath", str(attempt / "roms"),
               "-artpath", str(attempt / "artwork"),
               "-nvram_directory", str(attempt / "nvram"), "-cfg_directory", str(cfg),
               "-seconds_to_run", str(args.timeout), "-autoboot_delay", "0", "-autoboot_script", str(args.driver)]
    with (run / "mame.log").open("wb") as log, central_office(run, env, args.python):
        with subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT, cwd=run, env=env) as process:
            try:
                exit_code, timeout_reason = wait_for_mame(process, display, args.startup_timeout, args.timeout, not detached)
            finally:
                if process.poll() is None:
                    process.kill()
    entries = list(events(run / "display.txt.events"))
    final = next((item for item in reversed(entries) if item["event"] in ("passed", "failed", "timeout", "blocked", "power-cycle")), None)
    signals = {item["detail"] for item in entries if item["event"] == "observed"}
    log = read(run / "mame.log")
    packets = list(ncc_frames(log))
    json_file(run / "ncc-frames.json", packets)
    co_log = read(run / "co.log")
    prerequisites = "jack C1 line present" in co_log and bool(re.search(r"^jack H[01]$", co_log, re.M))
    infrastructure = not prerequisites and bool(entries) and not args.boot_only
    launch_error = os.name == "nt" and not log.strip() and exit_code is not None and exit_code >= 0xC0000000
    boot = []
    for line in read(run / "boot.jsonl").splitlines():
        try:
            boot.append(json.loads(line))
        except json.JSONDecodeError:
            continue
    milestones = list(dict.fromkeys(entry["milestone"] for entry in boot if "milestone" in entry))
    status = "fail"
    if infrastructure or launch_error:
        status = "blocked"
    elif exit_code is None or (final and final["event"] == "timeout"):
        status = "timeout"
    elif exit_code == 0 and final and final["event"] == "passed":
        status = "pass" if prerequisites or args.boot_only else "blocked"
    elif exit_code == 0 and final and final["event"] == "power-cycle":
        status = "power-cycle" if prerequisites else "blocked"
    elif final and final["event"] == "blocked":
        status = "blocked"
    elif exit_code == 0 and not prerequisites and not args.boot_only:
        status = "blocked"
    fatal = re.search(r"^Fatal error: (.+)", log, re.M)
    return {
        "status": status,
        "reason": (f"Windows stopped MAME before logging (0x{exit_code:08X})" if launch_error
                   else "local CO did not receive terminal hook state" if infrastructure else final["detail"] if final
                   else fatal[1] if fatal else timeout_reason if exit_code is None else "emulator exited"),
        "co_link_incomplete": infrastructure,
        "native_launch_error": launch_error,
        "exit_code": exit_code,
        "last_phase": next((item["detail"] for item in reversed(entries) if item["event"] == "phase"), None),
        "gates_observed": {name: message in signals for name, message in SIGNALS.items()},
        "download_display": sorted(signal for signal in signals if signal.startswith("download completed")),
        "co_connected": prerequisites, "boot_milestones": milestones,
        "initial_registers": next((entry["registers"] for entry in boot if "registers" in entry), None),
        "vfd_observed": any(item["event"] == "vfd" and re.search(r"[a-z]", item["detail"]) for item in entries),
        "ncc_frames": len(packets), "table_acks": [packet["table_ack"] for packet in packets if "table_ack" in packet],
        "rate_request_observed": any(packet["direction"] == "tx" and packet.get("leading_type") == 63 for packet in packets),
        "voiceware_trace": (run / "acceptance-trace-voiceware.jsonl").is_file(),
        "battery_restored": "battery load" in log,
        "flash_restored": "program flash load" in log,
    }


def attempt_install(args, settings, library, row, verified, program, board, hardware, attempt):
    attempt.mkdir(parents=True)
    (attempt / "nvram").mkdir()
    roms = attempt / "roms/cl_millennium"
    roms.mkdir(parents=True)
    shutil.copytree(ROOT / "fixtures", attempt / "fixtures")
    artwork = attempt / "artwork/cl_millennium"
    artwork.mkdir(parents=True)
    shutil.copyfile(ROOT / "src/mame/layout/millennium.lay", artwork / "default.lay")
    shutil.copyfile(ROOT / "artwork/millennium-terminal-front.png", artwork / "millennium-terminal-front.png")
    shutil.copyfile(ROOT / "artwork/Millenft.ttf", artwork.parent / "Millenft.ttf")
    cpu = attempt / "cpu"
    cpu.mkdir()
    pieces = []
    for index, item in enumerate(program):
        raw = (library / item["path"]).read_bytes()
        if hashlib.sha256(raw).hexdigest() != item["sha256"]:
            raise ValueError(f"Input changed after preflight: {item['path']}")
        (cpu / f"flash{index}.bin").write_bytes(raw)
        pieces.append(raw)
    image = b"".join(pieces)
    profile = json.loads(args.profile.read_text(encoding="utf-8"))
    rom_size = profile["memory"]["rom_size"]
    if len(image) == 262144 and rom_size > len(image):
        image = image.ljust(rom_size, b"\xff")
    if len(image) != rom_size:
        raise ValueError(f"This board profile requires {rom_size} program bytes, got {len(image)}")
    effective_hash = hashlib.sha256(image).hexdigest()
    json_file(attempt / "fixtures/firmware/firmware-hashes.json", {"entries": [{
        "version_id": program[0]["firmware_id"], "motherboard": board,
        "friendly_name": f"{program[0]['firmware_id']} mapped flash ({effective_hash[:12]})",
        "sha256_hex": effective_hash, "size_bytes": rom_size,
    }]})
    voice_source, voice = voice_pair(row, verified, args.reference)
    voice_records = []
    for filename, source in zip(("voice_u16.bin", "voice_u26.bin"), voice, strict=True):
        original = library / source
        shutil.copyfile(original, roms / filename)
        voice_records.append({"role": filename, "sha256": sha256(roms / filename)})
    env = {key: value for key, value in os.environ.items() if not key.upper().startswith("COINLINE_")}
    card_dma = PRESETS[hardware].card_dma_channel if hardware and board == "later" else 0
    env.update(COINLINE_EMU_ROOT=str(attempt), COINLINE_MOTHERBOARD=board, COINLINE_DIAGNOSTICS="0",
               COINLINE_HARDWARE_PRESET=hardware or "",
               COINLINE_CARD_DMA_CHANNEL=str(card_dma),
               COINLINE_ACCEPTANCE_COIN_BASIC="1" if hardware == "coin-basic" else "0",
               COINLINE_BOARD=str(args.profile),
               COINLINE_TP_BACKEND="pcd3349a", COINLINE_TP8048_ROM=str(args.controller),
               COINLINE_MAG_CARD=str(args.mag_card.resolve()),
               COINLINE_SMART_CARD=str(args.smart_card.resolve()),
               COINLINE_NCC_PORT=str(settings["ncc_port"]), COINLINE_CO_MODE="local", COINLINE_CO_TIMEOUT="0",
               COINLINE_MODEM_TRACE="1", COINLINE_OUTBOUND=settings.get("free_call", ""),
               COINLINE_ACCEPTANCE_LINE=settings["line"], COINLINE_ACCEPTANCE_SERIAL=settings["serial"],
               COINLINE_ACCEPTANCE_PIN=settings["pin"],
               COINLINE_ACCEPTANCE_KEYS="+-LN1234567890*#" + "abcdefghij"[:len(profile["keypad"]["quick_access_keys"])],
               COINLINE_ACCEPTANCE_DIAL=settings["free_call"] if (args.test_call or args.test_mag_call) else "",
               COINLINE_ACCEPTANCE_MAG_CALL="1" if args.test_mag_call else "0")
    if settings.get("code_server_number"):
        env.update(COINLINE_CODE_SERVER_NUMBER=settings["code_server_number"],
                   COINLINE_CODE_SERVER_PORT=str(settings["code_server_port"]))
    if len(program) == 1:
        env["COINLINE_FIRMWARE"] = str(cpu / "flash0.bin")
    else:
        for index in range(len(program)):
            env[f"COINLINE_FIRMWARE_FLASH{index}"] = str(cpu / f"flash{index}.bin")
    _, tag = program_tag(pieces[0])
    result = {"attempt": attempt.name, "board_mapping": board, "board_automatic": not args.board,
              "hardware_preset": hardware, "firmware_hardware_tag": tag, "card_dma_channel": card_dma,
              "effective_program_sha256": effective_hash,
              "program": [{"path": f["path"], "sha256": f["sha256"]} for f in program],
              "voiceware_source": voice_source, "voiceware": voice_records, "desk_key_card_supplied": "desk_key_card" in settings,
              "mag_card_sha256": sha256(args.mag_card), "smart_card_sha256": sha256(args.smart_card)}
    battery = attempt / "nvram/cl_millennium/battery"
    persisted_flash = attempt / "nvram/cl_millennium/program_flash"
    if hardware == "coin-basic" and not args.boot_only:
        preflight = run_phase(args, settings, attempt, "portable-preflight", env)
        result["portable_preflight"] = preflight
        if preflight["status"] not in ("pass", "power-cycle") or not battery.is_file():
            result["cold"] = preflight
            if preflight["status"] in ("pass", "power-cycle"):
                result["cold"] = dict(preflight, status="fail", reason="no battery image after portable-display preparation")
            result["status"] = result["cold"]["status"]
            json_file(attempt / "result.json", result)
            return result
        result["portable_preflight_battery_sha256"] = sha256(battery)
    result["cold"] = (preflight if hardware == "coin-basic" and not args.boot_only and preflight["status"] == "pass"
                      else run_phase(args, settings, attempt, "cold", env))
    if battery.is_file():
        result["cold_battery_sha256"] = sha256(battery)
    if (flash := flash_evidence(persisted_flash, effective_hash)) is not None:
        result["cold_flash"] = flash
    if (board == "later" and not args.boot_only and result["cold"]["status"] == "pass"
            and "cold_flash" not in result):
        result["cold"].update(status="fail", reason="no persistent program flash after cold INSTALL")
    if not args.boot_only and result["cold"]["status"] == "pass" and battery.is_file():
        result["warm"] = run_phase(args, settings, attempt, "warm", env)
        result["warm_battery_sha256"] = sha256(battery)
        if (flash := flash_evidence(persisted_flash, effective_hash)) is not None:
            result["warm_flash"] = flash
        if result["warm"]["status"] == "pass" and not result["warm"]["battery_restored"]:
            result["warm"].update(status="fail", reason="emulator did not report loading the cold battery image")
        if "cold_flash" in result and result["warm"]["status"] == "pass" and not result["warm"]["flash_restored"]:
            result["warm"].update(status="fail", reason="emulator did not report loading the persistent program flash")
        if hardware == "coin-basic" and args.test_call and result["warm"]["status"] == "pass":
            result["detached"] = run_phase(args, settings, attempt, "detached", env)
            if result["detached"]["status"] == "pass" and not result["detached"]["battery_restored"]:
                result["detached"].update(status="fail", reason="emulator did not load the installed battery image")
    result["status"] = (result.get("warm", {}).get("status", "fail") if not args.boot_only and result["cold"]["status"] == "pass"
                        else result["cold"]["status"])
    if "detached" in result and result["status"] == "pass":
        result["status"] = result["detached"]["status"]
    json_file(attempt / "result.json", result)
    return result


def normalize_manager_settings(settings):
    """Accept canonical lab fields or the equivalent Manager export fields."""
    result = dict(settings)
    for canonical, exported in (("line", "wireId"), ("ncc_port", "nccPort"), ("pin", "craftPin")):
        if canonical not in result and exported in result:
            result[canonical] = result[exported]
    return result


def validate_data_routes(settings):
    """Validate the same loopback routes in cold and interactive sessions."""
    settings.setdefault("ncc_port", 4000)
    if type(settings["ncc_port"]) is not int or not 1 <= settings["ncc_port"] <= 65535:
        raise ValueError("ncc_port must be an integer port on loopback")
    code_number = settings.get("code_server_number", "")
    code_port = settings.get("code_server_port", 0)
    if code_number or code_port:
        if (not isinstance(code_number, str) or not re.fullmatch(r"[0-9]{10}", code_number)
                or code_number == "2130706433" or type(code_port) is not int or not 1 <= code_port <= 65535):
            raise ValueError("Set a distinct 10-digit code_server_number and a valid code_server_port")
    return settings


def validate_terminal(settings, test_call):
    settings = normalize_manager_settings(settings)
    for key, pattern in (("line", r"[0-9]{10}"), ("serial", r"[0-9]{10}"), ("pin", r"[0-9]{5}")):
        if not isinstance(settings.get(key), str) or not re.fullmatch(pattern, settings[key]):
            raise ValueError(f"Set {key} in the local settings JSON as digits in a string")
    if "free_call" in settings and (not isinstance(settings["free_call"], str) or
                                   not re.fullmatch(r"[0-9]{10,11}", settings["free_call"])):
        raise ValueError("free_call must be a 10- or 11-digit destination served by the local CO")
    if test_call and "free_call" not in settings:
        raise ValueError("Set free_call for the optional local call test")
    validate_data_routes(settings)
    if "desk_key_card" in settings and not Path(settings["desk_key_card"]).is_file():
        raise ValueError("desk_key_card must point to a local synthetic magstripe fixture")
    return settings


def validate_settings(args, selected):
    contents = args.settings.read_bytes()
    settings = json.loads(contents)
    overrides = settings.pop("firmware_overrides", {})
    if not isinstance(overrides, dict) or not all(isinstance(value, dict) for value in overrides.values()):
        raise ValueError("firmware_overrides must map firmware IDs to lab terminal settings")
    for path in (args.mame, args.controller, args.python, args.profile,
                 *(args.reference / "roms/cl_millennium" / name for name in ("voice_u16.bin", "voice_u26.bin"))):
        if not path.is_file():
            raise ValueError(f"Missing run input: {path}")
    terminals = {code: validate_terminal({**settings, **overrides.get(code, {})}, args.test_call or args.test_mag_call) for code in selected}
    return terminals, hashlib.sha256(contents).hexdigest()


def retryable_attempt(args, settings, library, row, verified, program, board, hardware, attempt):
    for retry in (False, True):
        path = attempt.with_name(attempt.name + "-retry") if retry else attempt
        try:
            result = attempt_install(args, settings, library, row, verified, program, board, hardware, path)
        except (OSError, ValueError) as error:
            result = {"attempt": path.name, "board_mapping": board, "hardware_preset": hardware,
                      "status": "fail", "reason": str(error),
                      "cold": {"status": "fail", "last_phase": "setup or result collection",
                               "table_acks": [], "gates_observed": {"boot": False}}}
            path.mkdir(parents=True, exist_ok=True)
            json_file(path / "result.json", result)
        yield result
        cold = result["cold"]
        if not (cold.get("co_link_incomplete") or cold.get("native_launch_error")):
            return
        if not retry:
            print(f"Retrying infrastructure failure: {cold['reason']}", flush=True)


def write_summary(out, batch):
    json_file(out / "batch.json", batch)
    report = ["# Firmware acceptance results", "", f"Run mode: {batch['mode']}", "",
              "| Firmware | ROM board tag | Hardware preset / I/O | Card DMA | Portable prep | Cold | Warm | Detached call | Result | Last cold phase | ACKs |",
              "| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |"]
    registers = []
    for row in batch["results"]:
        if "attempts" not in row:
            report.append(f"| {row['id']} | - | - | - | - | - | - | - | {row['status']} | {row['reason']} | - |")
        for result in row.get("attempts", []):
            cold = result["cold"]
            acks = ", ".join(map(str, cold["table_acks"])) or "-"
            warm = result.get("warm", {}).get("status", "-")
            preflight = result.get("portable_preflight", {}).get("status", "-")
            detached = result.get("detached", {}).get("status", "-")
            case = f"{row['id']}/{result['attempt']}"
            hardware = f"{result.get('hardware_preset') or 'unidentified'} / {result['board_mapping']}"
            report.append(f"| [{case}]({case}/result.json) | {result.get('firmware_hardware_tag') or '-'} | {hardware} | "
                          f"{result.get('card_dma_channel', '-')} | {preflight} | {cold['status']} | {warm} | {detached} | {result['status']} | "
                          f"{cold['last_phase'] or 'setup'} | {acks} |")
            if initial := cold.get("initial_registers"):
                registers.append(f"| {case} | {initial.get('mmu', '-')} | {initial.get('asci', '-')} | "
                                 f"{initial.get('int', '-')} | {initial.get('dma', '-')} |")
    if registers:
        report.extend(["", "## Initial Z180 register snapshot", "",
                       "| Firmware | MMU | ASCI | Interrupts | DMA |", "| --- | --- | --- | --- | --- |", *registers])
    (out / "summary.md").write_text("\n".join(report) + "\n", encoding="utf-8")


def latest_mapping_attempts(record):
    return {item["attempt"].removesuffix("-retry"): item for item in record.get("attempts", [])}.values()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    library = ROOT / "build/firmware-acceptance"
    parser.add_argument("--library", type=Path, default=library)
    parser.add_argument("--settings", type=Path, default=library / "settings.json")
    parser.add_argument("--output", type=Path, default=library / "runs")
    parser.add_argument("--mame", type=Path, default=ROOT / ("third-party/mame/mamecoinline.exe" if os.name == "nt" else "third-party/mame/mamecoinline"))
    parser.add_argument("--controller", type=Path, default=ROOT / "firmware/tp_pcd3349a_behavioural.rom")
    parser.add_argument("--reference", type=Path, default=ROOT / "build/package-inputs/local")
    parser.add_argument("--profile", type=Path, default=ROOT / "fixtures/board/board-profile-2line-vfd.json")
    parser.add_argument("--mag-card", type=Path, default=ROOT / "fixtures/cards/magcard-valid.json")
    parser.add_argument("--smart-card", type=Path, default=ROOT / "fixtures/cards/smartcard-sync-test.json")
    parser.add_argument("--python", type=Path, default=Path(sys.executable), help="Python for the local CO sidecar")
    parser.add_argument("--only", action="append", help="firmware ID to run, repeatable")
    parser.add_argument("--skip", action="append", default=[], help="skip an ID covered by a separate batch")
    parser.add_argument("--board", choices=("earlier", "later", "both"), help="override the exploratory motherboard mapping")
    parser.add_argument("--hardware", choices=("auto", *PRESETS), default="auto", help="select or override the hardware preset")
    parser.add_argument("--timeout", type=int, default=1200, help="wall seconds allowed per cold or warm launch")
    parser.add_argument("--startup-timeout", type=int, default=90, help="wall seconds to wait for the first display")
    parser.add_argument("--hardware-trace", action="store_true", help="capture extra IRQ and MMU diagnostics")
    parser.add_argument("--io-trace", action="store_true", help="capture external I/O for board diagnosis")
    parser.add_argument("--uart-trace", action="store_true", help="capture external coin UART register accesses")
    parser.add_argument("--eeprom-trace", action="store_true", help="capture Microwire command and data transactions")
    parser.add_argument("--read-watch", type=Path, help="read-only physical memory watch list: label and hexadecimal address per line")
    parser.add_argument("--voice-trace", action="store_true", help="capture Voiceware commands and playback events")
    parser.add_argument("--card-trace", action="store_true", help="capture synchronous contact card pins")
    parser.add_argument("--telephony-trace", action="store_true", help="capture telephony controller messages and qualified CSIO")
    parser.add_argument("--inventory-only", action="store_true", help="verify hashes and report candidate plans without launching MAME")
    parser.add_argument("--boot-only", action="store_true", help="stop at the first stable startup prompt, without entering INSTALL")
    parser.add_argument("--test-call", action="store_true", help="after restart, dial the configured destination on the local CO")
    parser.add_argument("--test-mag-call", action="store_true", help="after restart, present a synthetic magstripe and dial the configured local-only test destination")
    args = parser.parse_args()
    if args.timeout <= 0 or args.startup_timeout <= 0:
        parser.error("Timeouts must be positive")
    if args.read_watch and not args.read_watch.is_file():
        parser.error("Read-watch file does not exist")
    if args.board == "both" and not args.boot_only:
        parser.error("--board both requires --boot-only")
    if args.test_call and args.test_mag_call:
        parser.error("Select either --test-call or --test-mag-call")
    if (args.test_call or args.test_mag_call) and (args.boot_only or args.inventory_only):
        parser.error("Local call testing requires an INSTALL run")
    for key in ("library", "output", "mame", "controller", "reference", "python", "profile", "mag_card", "smart_card"):
        setattr(args, key, getattr(args, key).resolve())
    if not args.inventory_only and (not args.mag_card.is_file() or not args.smart_card.is_file()):
        parser.error("Selected card fixture file does not exist")
    inventory_path = args.library / "inventory.json"
    if not inventory_path.is_file():
        inventory_path = Path(__file__).with_name("inventory.json")
    inventory = json.loads(inventory_path.read_text(encoding="utf-8"))
    rows = inventory["firmware"]
    selected = args.only or list(dict.fromkeys((*PRIORITY, *rows)))
    if unknown := (set(selected) | set(args.skip)) - rows.keys():
        parser.error("Unknown firmware IDs: " + ", ".join(sorted(unknown)))
    selected = [code for code in dict.fromkeys(selected) if code not in args.skip]
    if not selected:
        parser.error("No firmware IDs selected")
    if any(not re.fullmatch(r"[A-Za-z0-9.-]+", code) or code in {".", ".."} for code in selected):
        parser.error("Invalid firmware ID")
    settings, settings_hash = ({}, None) if args.inventory_only else validate_settings(args, selected)
    verified = inputs(args.library, inventory)
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    out = args.output / stamp
    out.mkdir(parents=True)
    args.driver = out / "install.lua"
    shutil.copyfile(Path(__file__).with_name("install.lua"), args.driver)
    if args.read_watch:
        watched = out / "read-watch.txt"
        shutil.copyfile(args.read_watch, watched)
        args.read_watch = watched
    json_file(out / "bin-hashes.json", list(verified.values()))
    (out / "SHA256SUMS").write_text("".join(f"{v['actual_sha256']}  {name}\n" for name, v in verified.items()
                                            if v["actual_sha256"]), encoding="utf-8", newline="\n")
    batch = {"started_utc": stamp, "firmware_files": len(verified),
             "mode": "inventory" if args.inventory_only else "boot probe" if args.boot_only else "install and restart",
             "verified_firmware_files": sum(v["verified"] for v in verified.values()),
             "emulator_sha256": None if args.inventory_only else sha256(args.mame),
             "controller_sha256": None if args.inventory_only else sha256(args.controller),
             "board_profile_sha256": None if args.inventory_only else sha256(args.profile),
             "settings_sha256": settings_hash,
             "panel_driver_sha256": sha256(args.driver), "local_call_test": args.test_call or args.test_mag_call,
             "magstripe_call_test": args.test_mag_call,
             "read_watch_sha256": sha256(args.read_watch) if args.read_watch else None,
             "selected": selected, "results": []}
    print(f"Results: {out}", flush=True)
    write_summary(out, batch)
    for code in selected:
        choices = list(candidates(rows[code], verified))
        if not choices:
            reason = "No supported program or verified flash pairing in the inventory" if rows[code]["images"] else "No dump available"
            record = {"id": code, "status": "blocked", "reason": reason}
        elif args.inventory_only:
            ready = sum(all(item["verified"] for item in program) for program, _ in choices)
            status = "planned" if ready else "blocked"
            record = {"id": code, "status": status,
                      "reason": f"{ready} of {len(choices)} program candidate(s) verified"}
        else:
            record = {"id": code, "attempts": []}
            for index, (program, board) in enumerate(choices, 1):
                if not all(item["verified"] for item in program):
                    record.setdefault("blocked_programs", []).append([item["path"] for item in program])
                    continue
                image = (args.library / program[0]["path"]).read_bytes()
                hardware, _ = select_hardware(args.hardware, image, code)
                if hardware:
                    board = PRESETS[hardware].motherboard
                mappings = (("earlier", "later") if args.board == "both" else (args.board,) if args.board else
                            (board,) if args.hardware != "auto" else
                            (board, "earlier") if board == "later" else (board,))
                for mapping in mappings:
                    print(f"Running {code} candidate {index} on {mapping} mapping", flush=True)
                    attempt = out / code / f"{index:02d}-{mapping}"
                    for result in retryable_attempt(args, settings[code], args.library, rows[code], verified, program, mapping,
                                                    hardware, attempt):
                        record["attempts"].append(result)
                        write_summary(out, {**batch, "results": [*batch["results"], record]})
                    if args.board != "both" and (result["cold"]["gates_observed"]["boot"] or result["status"] == "pass"):
                        break
            if not record["attempts"]:
                record = {"id": code, "status": "blocked", "reason": "Program checksum or size verification failed"}
        batch["results"].append(record)
        write_summary(out, batch)
        print(f"{code}: {record.get('status') or ', '.join(r['status'] for r in record['attempts'])}", flush=True)
    failed = any((args.only and row.get("status") == "blocked") or any(
        item["status"] != "pass" for item in latest_mapping_attempts(row)) for row in batch["results"])
    batch["finished_utc"] = datetime.now(timezone.utc).isoformat()
    write_summary(out, batch)
    return int(batch["verified_firmware_files"] != batch["firmware_files"] or
               (not args.inventory_only and failed))


if __name__ == "__main__":
    raise SystemExit(main())
