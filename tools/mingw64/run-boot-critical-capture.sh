#!/usr/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Windowed boot-critical capture + validate-boot-milestones (invoke PowerShell as script runner only).
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EMU_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$EMU_ROOT"

export MSYSTEM="${MSYSTEM:-MINGW64}"
export CHERE_INVOKING=1
export PATH="/mingw64/bin:/usr/bin:/bin:${PATH}"

if ! command -v cygpath >/dev/null 2>&1; then
	echo "ERROR: cygpath not found — use MSYS2 bash."
	exit 1
fi

TS="$(date +%Y%m%dT%H%M%S)"
RUN_KIND="${COINLINE_RUN_KIND:-boot-critical}"
RUN="${EMU_ROOT}/build/runs/${TS}-${RUN_KIND}"
mkdir -p "$RUN"
TRACE_PROFILE="${COINLINE_TRACE_PROFILE:-uart}"
RUN_SECONDS="${COINLINE_BOOT_CAPTURE_SECONDS:-30}"
RUN_WAV_WRITE="${COINLINE_BOOT_CAPTURE_WAV_WRITE:-${COINLINE_WAVWRITE:-0}}"
if [[ -z "$RUN_SECONDS" || ! "$RUN_SECONDS" =~ ^[0-9]+$ ]]; then
	echo "ERROR: COINLINE_BOOT_CAPTURE_SECONDS must be an integer number of seconds."
	exit 1
fi
RUN_HEADLESS="${COINLINE_BOOT_CAPTURE_HEADLESS:-0}"
RUN_AUDIO="${COINLINE_BOOT_CAPTURE_AUDIO:-1}"
RUN_REAL_INPUT_DEMO="${COINLINE_REAL_INPUT_DEMO:-0}"

while [[ $# -gt 0 ]]; do
	case "$1" in
	-TraceProfile) TRACE_PROFILE="${2:-}"; shift 2 ;;
	-RunSeconds) RUN_SECONDS="${2:-}"; shift 2 ;;
	-WavWrite) RUN_WAV_WRITE=1; shift ;;
	-EnableAudio) RUN_AUDIO=1; shift ;;
	-RealInputDemo) RUN_REAL_INPUT_DEMO=1; shift ;;
	-Screenshot) RUN_HEADLESS=0; shift ;;
	-Headless) RUN_HEADLESS=1; shift ;;
	*) echo "ERROR: unknown arg $1"; exit 1 ;;
	esac
done
case "$TRACE_PROFILE" in
fast|m6|uart|voice|full) ;;
*) echo "ERROR: TraceProfile must be one of fast|m6|uart|voice|full"; exit 1 ;;
esac
export COINLINE_TRACE_PROFILE="$TRACE_PROFILE"

# Boot-critical tracing (paths resolved by run-screenshot-capture.ps1 into run folder)
export COINLINE_TRACE_STACK=0
export COINLINE_TRACE_RAM_INIT=0
export COINLINE_TRACE_MMU_TRANSLATION=0
export COINLINE_TRACE_INTERRUPTS=0
export COINLINE_TRACE_TIMERS=0
export COINLINE_TRACE_ASCI=0
export COINLINE_TRACE_RESET=0
export COINLINE_TRACE_VECTOR_EVENTS=0
export COINLINE_TRACE_FETCH_PROVENANCE=0
export COINLINE_TRACE_STACK_CONTROL_FLOW=0

if [[ "$TRACE_PROFILE" == "m6" || "$TRACE_PROFILE" == "full" ]]; then
	export COINLINE_TRACE_VECTOR_EVENTS=1
	export COINLINE_TRACE_FETCH_PROVENANCE=1
fi
if [[ "$TRACE_PROFILE" == "uart" || "$TRACE_PROFILE" == "full" ]]; then
	export COINLINE_TRACE_ASCI=1
fi
if [[ "$TRACE_PROFILE" == "full" ]]; then
	export COINLINE_TRACE_STACK=1
	export COINLINE_TRACE_RAM_INIT=1
	export COINLINE_TRACE_MMU_TRANSLATION=1
	export COINLINE_TRACE_INTERRUPTS=1
	export COINLINE_TRACE_TIMERS=1
	export COINLINE_TRACE_RESET=1
	export COINLINE_TRACE_STACK_CONTROL_FLOW=1
fi

EMU_WIN="$(cygpath -aw "$EMU_ROOT")"
RUN_WIN="$(cygpath -aw "$RUN")"
CAP_WIN="$(cygpath -aw "$EMU_ROOT/tools/windows/run-screenshot-capture.ps1")"
VAL_WIN="$(cygpath -aw "$EMU_ROOT/tools/windows/validate-boot-milestones.ps1")"
PARENT_ROOT="$(cd "$EMU_ROOT/.." && pwd)"
FW_WIN="$(cygpath -aw "${COINLINE_FIRMWARE:-$PARENT_ROOT/firmware/flash.bin}")"
FWS_WIN="$(cygpath -aw "${COINLINE_FIRMWARE_SOURCE_ROOT:-$PARENT_ROOT}")"
U16_WIN="$(cygpath -aw "${EMU_ROOT}/../firmware/voice_a.bin")"
U26_WIN="$(cygpath -aw "${EMU_ROOT}/../firmware/voice_b.bin")"

set +e
PS_EXE="$(command -v powershell.exe 2>/dev/null || true)"
if [[ -z "$PS_EXE" && -x /c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe ]]; then
	PS_EXE="/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe"
fi
if [[ -z "$PS_EXE" ]]; then
	echo "ERROR: powershell.exe not found (needed to run $CAP_WIN). Add it to PATH or install Windows PowerShell."
	exit 1
fi

"$PS_EXE" -NoProfile -ExecutionPolicy Bypass -File "$CAP_WIN" \
	-FirmwareBinary "$FW_WIN" \
	-FirmwareSourceRoot "$FWS_WIN" \
	-HostUrl "http://127.0.0.1:5000" \
	-RunSeconds "$RUN_SECONDS" \
	-OutputDir "$RUN_WIN" \
	-TraceProfile "$TRACE_PROFILE" \
	-VoicewareTrace \
	-VoiceRomU16 "$U16_WIN" \
	-VoiceRomU26 "$U26_WIN" \
	-VoiceRomLayout banked_two_roms \
	$(if [[ "$RUN_HEADLESS" == "1" ]]; then printf '%s' '-Headless'; else printf '%s' '-Screenshot'; fi) \
	$(if [[ "$RUN_AUDIO" == "1" ]]; then printf '%s' '-EnableAudio'; fi) \
	$(if [[ "$RUN_WAV_WRITE" == "1" ]]; then printf '%s' '-WavWrite'; fi) \
	$(if [[ "$RUN_REAL_INPUT_DEMO" == "1" ]]; then printf '%s' '-RealInputDemo'; fi)
CAP_EXIT=$?
set -e

"$PS_EXE" -NoProfile -ExecutionPolicy Bypass -File "$VAL_WIN" -RunDir "$RUN_WIN" || true

echo "$RUN" > "${EMU_ROOT}/build/runs/latest-boot-critical.txt"

if [[ ! -f "$RUN/run.log" ]]; then
	echo "ERROR: run.log missing — MAME run did not produce evidence."
	exit 1
fi
if [[ $CAP_EXIT -ne 0 ]]; then
	echo "WARNING: run-screenshot-capture.ps1 exit $CAP_EXIT (see $RUN/run.log)"
	exit "$CAP_EXIT"
fi
echo "OK: boot-critical run folder $RUN"

# Per-run gate classification + cycle notes (required by boot-fix loop / audits)
RUN_DIR="$RUN" python3 <<'PY'
import json, os, pathlib, collections
run = os.environ["RUN_DIR"]
r = pathlib.Path(run)
o = {
    "schema_version": "coinline.boot_gate_classification/v1",
    "run_dir": str(r).replace("\\", "/"),
    "highest_milestone": "none",
    "port_0x60_vfd_data": False,
    "primary_gate": "unknown_hot_loop",
}
ms = []
if (r / "boot-trace.jsonl").is_file():
    for line in (r / "boot-trace.jsonl").read_text(encoding="utf-8", errors="replace").splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            m = json.loads(line).get("milestone")
            if m:
                ms.append(m)
        except Exception:
            pass
    if ms:
        o["highest_milestone"] = ms[-1]
has60 = False
if (r / "io-trace.jsonl").is_file():
    for line in (r / "io-trace.jsonl").open(encoding="utf-8", errors="replace"):
        if "vfd_data" in line or ('"port":"0x0060"' in line and '"rw":"w"' in line):
            has60 = True
            break
o["port_0x60_vfd_data"] = has60
g = "unknown_hot_loop"
if has60:
    g = "m6_candidate"
elif o["highest_milestone"] in ("M5V", "M5A", "M5C"):
    g = "upd7759_playback_binding"
elif o["highest_milestone"] in ("M5", "M4", "M3"):
    g = "scheduler_interrupt_timer"
o["primary_gate"] = g
(r / "boot-gate-classification.json").write_text(json.dumps(o, indent=2) + "\n", encoding="utf-8")
rec = f"""# Next fix recommendation

- **Run:** `{run}`
- **Capture seconds:** {os.environ.get('COINLINE_BOOT_CAPTURE_SECONDS', '30')}
- **Highest milestone:** {o['highest_milestone']}
- **Port 0x60 vfd_data write:** {has60}
- **Primary gate:** {g}
- **Next action:** If still at M5V, verify uPD7759 slave/standalone path and phrase completion; then VFD/scheduler.
"""
(r / "next-fix-recommendation.md").write_text(rec, encoding="utf-8")
(r / "cycle-summary.md").write_text(
    f"boot-critical {os.environ.get('COINLINE_BOOT_CAPTURE_SECONDS', '30')}s capture\nrun={run}\nhighest={o['highest_milestone']}\n0x60={has60}\ngate={g}\n",
    encoding="utf-8",
)

# Phase-2 style loop analytics artifacts
def read_jsonl(path):
    if not path.is_file():
        return []
    out = []
    for ln in path.read_text(encoding="utf-8", errors="replace").splitlines():
        ln = ln.strip()
        if not ln:
            continue
        try:
            out.append(json.loads(ln))
        except Exception:
            pass
    return out

cpu_rows = read_jsonl(r / "cpu-trace.jsonl")
io_rows = read_jsonl(r / "io-trace.jsonl")
voice_rows = read_jsonl(r / "voiceware-trace.jsonl")

pcs = []
sp_vals = []
trans = collections.Counter()
pc_freq = collections.Counter()
for row in cpu_rows:
    pc = row.get("pc")
    sp = row.get("sp")
    if isinstance(pc, str):
        pcs.append(pc)
        pc_freq[pc] += 1
    if isinstance(sp, str):
        try:
            sp_vals.append(int(sp, 16))
        except Exception:
            pass
for i in range(1, len(pcs)):
    trans[f"{pcs[i-1]}->{pcs[i]}"] += 1

hot_pc = [{"pc": pc, "count": cnt} for pc, cnt in pc_freq.most_common(30)]
hot_bb = [{"edge": e, "count": c} for e, c in trans.most_common(30)]
(r / "hot-pc-frequency.json").write_text(json.dumps({
    "schema_version": "coinline.hot_pc_frequency/v1",
    "run_dir": str(r).replace("\\", "/"),
    "top_pcs": hot_pc,
}, indent=2) + "\n", encoding="utf-8")
(r / "hot-basic-blocks.json").write_text(json.dumps({
    "schema_version": "coinline.hot_basic_blocks/v1",
    "run_dir": str(r).replace("\\", "/"),
    "top_edges": hot_bb,
}, indent=2) + "\n", encoding="utf-8")

def bool_repeat(pattern):
    return any((pattern in e["edge"]) for e in hot_bb)

voice_cmd_cycles = [int(x.get("cycle", 0)) for x in io_rows if x.get("port") == "0x0061" and x.get("rw") == "w" and x.get("data") == "0xB3"]
voice_intervals = [voice_cmd_cycles[i] - voice_cmd_cycles[i - 1] for i in range(1, len(voice_cmd_cycles))]
idle_edges = 0
for x in voice_rows:
    if x.get("event_type") == "voice_segment_start" and x.get("upd7759_idle_before") is True and x.get("upd7759_idle_after") is True:
        idle_edges += 1

sp_profile = "unknown"
if sp_vals:
    sp_min = min(sp_vals)
    sp_max = max(sp_vals)
    span = sp_max - sp_min
    sp_profile = "stable" if span < 0x40 else ("oscillating" if span < 0x2000 else "wide_drift")
else:
    sp_min = sp_max = None

(r / "voice-loop-signature.json").write_text(json.dumps({
    "schema_version": "coinline.voice_loop_signature/v1",
    "port_0x61_0xB3_repeat": len(voice_cmd_cycles) > 3,
    "port_0x61_0xB3_count": len(voice_cmd_cycles),
    "voice_cmd_intervals_sample": voice_intervals[:30],
    "upd7759_idle_start_edges": idle_edges,
    "pc_0x5B29_repeats": pc_freq.get("0x5B29", 0),
}, indent=2) + "\n", encoding="utf-8")

(r / "scheduler-loop-signature.json").write_text(json.dumps({
    "schema_version": "coinline.scheduler_loop_signature/v1",
    "pc_0x00CF_to_0x00E5_repeats": bool_repeat("0x00CF->0x00E5") or bool_repeat("0x00E5->"),
    "pc_0x0038_repeats": pc_freq.get("0x0038", 0),
    "stack_pointer_profile": sp_profile,
    "sp_min": sp_min,
    "sp_max": sp_max,
}, indent=2) + "\n", encoding="utf-8")
PY

