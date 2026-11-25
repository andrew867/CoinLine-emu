#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Measure native machine-clock progress while a Windows window frame is captured."""

import argparse
import ctypes as C
from ctypes import wintypes as W
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time

from run import ROOT


WM_CLOSE, WM_CANCELMODE = 0x0010, 0x001F
WM_NCMOUSEMOVE, WM_NCLBUTTONDOWN, WM_NCLBUTTONUP = 0x00A0, 0x00A1, 0x00A2
HTCAPTION, HTBOTTOMRIGHT = 2, 17
GUI_INMOVESIZE = 2


class GUIThreadInfo(C.Structure):
    _fields_ = [("cbSize", W.DWORD), ("flags", W.DWORD), ("hwndActive", W.HWND),
                ("hwndFocus", W.HWND), ("hwndCapture", W.HWND), ("hwndMenuOwner", W.HWND),
                ("hwndMoveSize", W.HWND), ("hwndCaret", W.HWND), ("rcCaret", W.RECT)]


LUA = '''local output = assert(io.open(assert(os.getenv("COINLINE_WINDOW_CLOCK")), "w"))
local next_sample = 0
emu.register_frame_done(function()
    local now = emu.time()
    if now >= next_sample then
        output:write(string.format("%.9f\\n", now))
        output:flush()
        next_sample = now + 0.10
    end
end)
'''


def wait_for(predicate, seconds, description):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        result = predicate()
        if result:
            return result
        time.sleep(0.05)
    raise RuntimeError(f"Timed out waiting for {description}")


class Window:
    def __init__(self, process):
        self.api = C.WinDLL("user32", use_last_error=True)
        self.enumproc = C.WINFUNCTYPE(W.BOOL, W.HWND, W.LPARAM)
        self.api.EnumWindows.argtypes = [self.enumproc, W.LPARAM]
        self.api.GetWindowThreadProcessId.argtypes = [W.HWND, C.POINTER(W.DWORD)]
        self.api.GetWindowThreadProcessId.restype = W.DWORD
        self.api.GetClassNameW.argtypes = [W.HWND, W.LPWSTR, C.c_int]
        self.api.IsWindowVisible.argtypes = [W.HWND]
        self.api.GetGUIThreadInfo.argtypes = [W.DWORD, C.POINTER(GUIThreadInfo)]
        self.api.GetWindowRect.argtypes = [W.HWND, C.POINTER(W.RECT)]
        self.api.PostMessageW.argtypes = [W.HWND, W.UINT, W.WPARAM, W.LPARAM]
        self.api.PostMessageW.restype = W.BOOL

        def find():
            if process.poll() is not None:
                raise RuntimeError(f"Native process exited {process.returncode}")
            handles = []

            @self.enumproc
            def visit(hwnd, unused):
                pid = W.DWORD()
                thread = self.api.GetWindowThreadProcessId(hwnd, C.byref(pid))
                if pid.value == process.pid and self.api.IsWindowVisible(hwnd):
                    name = C.create_unicode_buffer(32)
                    self.api.GetClassNameW(hwnd, name, len(name))
                    if name.value == "MAME":
                        handles.append((hwnd, thread))
                return True

            self.api.EnumWindows(visit, 0)
            return handles[0] if handles else None

        self.hwnd, self.thread = wait_for(find, 20, "visible native MAME window")

    def bounds(self):
        rect = W.RECT()
        if not self.api.GetWindowRect(self.hwnd, C.byref(rect)):
            raise C.WinError(C.get_last_error())
        return (rect.left, rect.top, rect.right, rect.bottom)

    def captured(self):
        state = GUIThreadInfo(cbSize=C.sizeof(GUIThreadInfo))
        if not self.api.GetGUIThreadInfo(self.thread, C.byref(state)):
            raise C.WinError(C.get_last_error())
        if state.flags & GUI_INMOVESIZE:
            raise RuntimeError("The native modal move/size loop was entered")
        return state.hwndCapture == self.hwnd

    def post(self, msg, hit=0, point=(0, 0)):
        x, y = point
        pos = (x & 0xFFFF) | ((y & 0xFFFF) << 16)
        if not self.api.PostMessageW(self.hwnd, msg, hit, pos):
            raise C.WinError(C.get_last_error())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mame", type=Path, required=True)
    parser.add_argument("--display", choices=("2line", "11line"), default="11line")
    parser.add_argument("--firmware", type=Path, help="Optional 1 MiB later-board mapped firmware image, not modified")
    parser.add_argument("--rompath", type=Path, default=ROOT / "build/package-inputs/local/roms")
    parser.add_argument("--controller", type=Path, default=ROOT / "firmware/tp_pcd3349a_behavioural.rom")
    parser.add_argument("--sound", action="store_true", help="Enable native audio output")
    parser.add_argument("--video", choices=("gdi", "d3d", "opengl"), default="gdi")
    parser.add_argument("--frameskip", type=int, choices=range(12), default=4, help="Maximum automatic frameskip")
    parser.add_argument("--free-aspect", action="store_true", help="Disable aspect constraints for the resize geometry test")
    parser.add_argument("--output", type=Path, default=ROOT / "build/firmware-acceptance/window-drag-probe")
    args = parser.parse_args()
    if os.name != "nt":
        parser.error("This is a Win32 OSD qualification probe")

    out = args.output.resolve() / datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    out.mkdir(parents=True)
    shutil.copytree(ROOT / "fixtures", out / "fixtures")
    if args.firmware:
        image = args.firmware.read_bytes()
        if len(image) != 1048576:
            parser.error("Supply a mapped 1 MiB later-board firmware image")
    else:
        # Execute a continuous Z180 instruction loop through the native CPU.
        image = bytearray(b"\xff" * 1048576)
        image[0xc0000:0xc0003] = b"\xf3\x18\xfe"
    flash = out / "flash.bin"
    flash.write_bytes(image)
    digest = hashlib.sha256(image).hexdigest()
    registry = out / "fixtures/firmware/firmware-hashes.json"
    data = json.loads(registry.read_text(encoding="utf-8"))
    if not any(entry["sha256_hex"] == digest for entry in data["entries"]):
        data["entries"].append({"version_id": "window-drag-probe", "friendly_name": "Window timing probe",
                                "sha256_hex": digest, "size_bytes": len(image), "motherboard": "later"})
        registry.write_text(json.dumps(data), encoding="utf-8")

    clock = out / "machine-clock.txt"
    script = out / "clock.lua"
    script.write_text(LUA, encoding="utf-8")
    env = {key: value for key, value in os.environ.items() if not key.upper().startswith("COINLINE_")}
    env.update(COINLINE_EMU_ROOT=str(out), COINLINE_MOTHERBOARD="later", COINLINE_HARDWARE_PRESET="multipay-v1.3",
               COINLINE_BOARD=str(ROOT / f"fixtures/board/board-profile-{args.display}-vfd.json"),
               COINLINE_FIRMWARE=str(flash), COINLINE_DIAGNOSTICS="0", COINLINE_LINE_PORT="0",
               COINLINE_TP8048_ROM=str(args.controller.resolve()), COINLINE_WINDOW_CLOCK=str(clock))
    command = [str(args.mame.resolve()), "cl_millennium", "-noreadconfig", "-skip_gameinfo", "-nodebug", "-nolog",
               "-window", "-nomaximize", "-resolution", "360x850", "-video", args.video, "-autoframeskip", "-frameskip", str(args.frameskip),
               "-view", "Graphical" if args.display == "11line" else "Standard", "-sound", "auto" if args.sound else "none",
               "-rompath", str(args.rompath.resolve()), "-artpath", str(ROOT / "artwork"),
               "-nvram_directory", str(out / "nvram"), "-cfg_directory", str(out / "cfg"),
               "-autoboot_delay", "0", "-autoboot_script", str(script)]
    if args.free_aspect:
        command.append("-nokeepaspect")
    result = {"firmware_sha256": digest, "synthetic_cpu_loop": args.firmware is None,
              "display": args.display, "audio_enabled": args.sound, "video": args.video,
              "automatic_frameskip_max": args.frameskip, "keep_aspect": not args.free_aspect,
              "intervals": [], "passed": False}

    def read_clock():
        if not clock.is_file():
            return None
        try:
            lines = clock.read_text(encoding="ascii").splitlines()
        except PermissionError:
            return None
        return float(lines[-1]) if lines else None

    def observe(label, window, captured, seconds=3.0):
        start = time.monotonic()
        first = last = read_clock()
        if first is None:
            raise RuntimeError("Machine clock not available")
        last_progress = start
        max_gap = 0.0
        while (now := time.monotonic()) - start < seconds:
            if window.captured() != captured:
                raise RuntimeError(f"Unexpected mouse capture state during {label}")
            sample = read_clock()
            if sample is not None and sample != last:
                max_gap = max(max_gap, now - last_progress)
                last, last_progress = sample, now
            time.sleep(0.05)
        end = time.monotonic()
        max_gap = max(max_gap, end - last_progress)
        delta = last - first
        row = {"phase": label, "wall_seconds": end - start, "emulated_seconds": delta,
               "speed_ratio": delta / (end - start), "max_observed_clock_gap_seconds": max_gap,
               "mouse_captured": captured}
        result["intervals"].append(row)
        print(json.dumps(row), flush=True)
        return row["speed_ratio"]

    with (out / "mame.log").open("wb") as log:
        process = subprocess.Popen(command, cwd=out, env=env, stdout=log, stderr=subprocess.STDOUT)
        window = None
        try:
            window = Window(process)
            wait_for(lambda: (read_clock() or 0) >= 2.0, 30, "native machine clock")
            idle = observe("idle", window, False)

            for label, hit, dx, dy in (("caption", HTCAPTION, 40, 30), ("resize", HTBOTTOMRIGHT, -60, -80)):
                before = window.bounds()
                origin = (before[0] + 80, before[1] + 12) if hit == HTCAPTION else (before[2] - 2, before[3] - 2)
                target = (origin[0] + dx, origin[1] + dy)
                # Post targeted frame messages, not global mouse input. The Win32
                # capture and geometry are still exercised in the native window.
                window.post(WM_NCLBUTTONDOWN, hit, origin)
                wait_for(window.captured, 3, f"{label} mouse capture")
                window.post(WM_NCMOUSEMOVE, hit, target)
                # The default layout can have identical min/max dimensions on a
                # small work area. The unconstrained geometry test is independent
                # of those MAME aspect limits.
                if hit == HTCAPTION or args.free_aspect:
                    wait_for(lambda: window.bounds() != before, 3, f"{label} geometry update")
                rate = observe(label, window, True)
                window.post(WM_NCLBUTTONUP, hit, target)
                wait_for(lambda: not window.captured(), 3, f"{label} release")
                after = window.bounds()
                result[f"{label}_bounds"] = {"before": before, "after": after}
                if hit == HTCAPTION:
                    if tuple(a - b for a, b in zip(after, before)) != (dx, dy, dx, dy):
                        raise RuntimeError("Incorrect final caption geometry")
                elif args.free_aspect and (after[2] - after[0] >= before[2] - before[0] or after[3] - after[1] >= before[3] - before[1]):
                    raise RuntimeError("Resize did not decrease both dimensions")
                if rate < max(0.2, idle * 0.5):
                    raise RuntimeError(f"Machine clock did not make substantial progress during {label}")

            before = window.bounds()
            window.post(WM_NCLBUTTONDOWN, HTCAPTION, (before[0] + 80, before[1] + 12))
            wait_for(window.captured, 3, "cancellation capture")
            window.post(WM_CANCELMODE)
            wait_for(lambda: not window.captured(), 3, "cancellation release")
            recovery = observe("recovery", window, False)
            result["passed"] = idle >= 0.2 and recovery >= max(0.2, idle * 0.5)
            if not result["passed"]:
                raise RuntimeError("Idle or recovered machine clock too slow for qualification")
        except Exception as error:
            result["error"] = str(error)
            raise
        finally:
            if process.poll() is None:
                if window is not None:
                    window.post(WM_CANCELMODE)
                    window.post(WM_CLOSE)
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.terminate()
                    process.wait(timeout=5)
            result["native_exit_code"] = process.returncode
            result["passed"] = result["passed"] and process.returncode == 0
            (out / "result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
            print(f"Window drag qualification evidence: {out}", flush=True)
    if not result["passed"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
