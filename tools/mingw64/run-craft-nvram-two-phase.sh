#!/usr/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Two-phase craft-style run: (1) blank JSON NVRAM image + empty MAME nvram_directory —
#     first boot uses COINLINE_NVRAM; firmware/RTOS should program persistence.
# (2) Same -nvram_directory, no COINLINE_NVRAM — MAME loads saved .nv; NVRAM storage trace
#     should show non-zero validity reads if phase 1 wrote and saved.
#
# Optional: COINLINE_CRAFT_RUN_SECONDS — hard cap per phase (MAME -seconds_to_run). Default 75.
# Microwire JSONL idle early-exit (faster when EEPROM traffic finishes early):
#   COINLINE_MICROWIRE_EARLY_IDLE_SEC   default 6 (stable file size for this many seconds); set 0 to disable
#   COINLINE_MICROWIRE_EARLY_MIN_LINES    default 8 (require at least this many JSONL lines before idle can trigger)
#   COINLINE_MICROWIRE_EARLY_WARM_SEC     default 4 (boot screenshot delay before idle polling)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EMU_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$EMU_ROOT"

export MSYSTEM="${MSYSTEM:-MINGW64}"
export CHERE_INVOKING=1
export PATH="/mingw64/bin:/usr/bin:/bin:${PATH}"

if ! command -v cygpath >/dev/null 2>&1; then
	echo "ERROR: cygpath not found — run in MSYS2/MINGW64."
	exit 1
fi

PS_EXE="$(command -v powershell.exe 2>/dev/null || true)"
if [[ -z "$PS_EXE" && -x /c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe ]]; then
	PS_EXE="/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe"
fi
if [[ -z "$PS_EXE" ]]; then
	echo "ERROR: powershell.exe not found"
	exit 1
fi

ZERO_JSON="${COINLINE_NVRAM_ALL_ZERO:-$EMU_ROOT/fixtures/nvram/all-zero.nvram.json}"
if [[ ! -f "$ZERO_JSON" ]]; then
	echo "ERROR: missing $ZERO_JSON (run: python3 tools/nvram/gen_all_zero_nvram_json.py)"
	exit 2
fi

TS="$(date +%Y%m%dT%H%M%S)"
RUN="${EMU_ROOT}/build/runs/${TS}-craft-nvram-two-phase"
mkdir -p "$RUN"
NVPERSIST="$RUN/mame-nvram"
mkdir -p "$NVPERSIST"
find "$NVPERSIST" -name '*.nv' -delete 2>/dev/null || true

PARENT_ROOT="$(cd "$EMU_ROOT/.." && pwd)"
FW_DIR="$PARENT_ROOT/firmware"
if [[ -z "${COINLINE_FIRMWARE_FLASH1:-}" ]] && [[ -f "$FW_DIR/flash.bin" ]] && [[ -f "$FW_DIR/flash1.bin" ]]; then
	export COINLINE_FIRMWARE_FLASH0="$FW_DIR/flash.bin"
	export COINLINE_FIRMWARE_FLASH1="$FW_DIR/flash1.bin"
fi
if [[ -n "${COINLINE_FIRMWARE_FLASH1:-}" ]]; then
	FW_PRIMARY="${COINLINE_FIRMWARE_FLASH0:-${COINLINE_FIRMWARE:-$FW_DIR/flash.bin}}"
else
	FW_PRIMARY="${COINLINE_FIRMWARE:-$FW_DIR/flash.bin}"
fi
FW_WIN="$(cygpath -aw "$FW_PRIMARY")"
FWS_WIN="$(cygpath -aw "${COINLINE_FIRMWARE_SOURCE_ROOT:-$PARENT_ROOT}")"

U16_PATH="${COINLINE_VOICE_ROM_A:-${COINLINE_VOICE_ROM_U16:-$PARENT_ROOT/firmware/voice_a.bin}}"
U26_PATH="${COINLINE_VOICE_ROM_B:-${COINLINE_VOICE_ROM_U26:-$PARENT_ROOT/firmware/voice_b.bin}}"
U16_WIN="$(cygpath -aw "$U16_PATH")"
U26_WIN="$(cygpath -aw "$U26_PATH")"

RUN_WIN="$(cygpath -aw "$RUN")"
NVP_WIN="$(cygpath -aw "$NVPERSIST")"
ZERO_WIN="$(cygpath -aw "$ZERO_JSON")"
CAP_WIN="$(cygpath -aw "$EMU_ROOT/tools/windows/run-screenshot-capture.ps1")"

SEC="${COINLINE_CRAFT_RUN_SECONDS:-75}"

if [[ "${COINLINE_MICROWIRE_EARLY_IDLE_SEC:-}" == "0" ]]; then
	EXTRA_PS_EARLY=()
else
	EARLY_IDLE="${COINLINE_MICROWIRE_EARLY_IDLE_SEC:-6}"
	EARLY_MIN="${COINLINE_MICROWIRE_EARLY_MIN_LINES:-8}"
	EARLY_WARM="${COINLINE_MICROWIRE_EARLY_WARM_SEC:-4}"
	EXTRA_PS_EARLY=(
		"-MicrowireEarlyStopIdleSeconds"
		"$EARLY_IDLE"
		"-MicrowireEarlyStopMinJsonlLines"
		"$EARLY_MIN"
		"-MicrowireEarlyStopWarmSeconds"
		"$EARLY_WARM"
	)
	# Do not exit early on read-only EEPROM idle bursts; wait for a committed Microwire write unless disabled.
	if [[ "${COINLINE_MICROWIRE_EARLY_REQUIRE_WRITE:-1}" == "1" ]]; then
		EXTRA_PS_EARLY+=("-MicrowireEarlyStopRequireCommittedWrite" '$true')
	fi
fi

# Prefer PCD3349A/TP8048 CSI/O path for install/craft fidelity (override with COINLINE_TP_BACKEND=legacy).
export COINLINE_TP_BACKEND="${COINLINE_TP_BACKEND:-pcd3349a}"

# Trace profile similar to craft entry (UART-weighted).
export COINLINE_TRACE_PROFILE="${COINLINE_TRACE_PROFILE:-uart}"
export COINLINE_TRACE_ASCI=1
export COINLINE_TRACE_TELEPHONY=1
export COINLINE_TRACE_PANEL=1
export COINLINE_ACCEPTANCE_MODE=1
export COINLINE_CRAFT_ONLY_MODE=1
export COINLINE_SCRIPTED_PANEL_DEMO=1

# EEPROM/NVRAM JSONL gates require microwire + nvram-storage sinks. A stray COINLINE_TRACE_ONLY
# (shell or Windows user env) keeps only one filename match and clears these paths in the driver.
unset COINLINE_TRACE_ONLY 2>/dev/null || true
export COINLINE_UNSET_TRACE_ONLY=1

phase1_dir="$RUN/phase1-first-boot"
phase2_dir="$RUN/phase2-persisted-nv"
mkdir -p "$phase1_dir" "$phase2_dir"

# --- Phase 1: blank envelope + no saved .nv ---
export COINLINE_NVRAM="$ZERO_JSON"
# Win32 MAME opens trace paths via narrow APIs; use Windows paths (not MSYS /c/...).
export COINLINE_NVRAM_STORAGE_TRACE="$(cygpath -w "$phase1_dir/nvram-storage-trace.jsonl")"
export COINLINE_MICROWIRE_TRACE="$(cygpath -w "$phase1_dir/microwire-eeprom-trace.jsonl")"
export COINLINE_TRACE_MICROWIRE=1
export COINLINE_TRACE_NVRAM_STORAGE=1
: >"$phase1_dir/nvram-storage-trace.jsonl"
: >"$phase1_dir/microwire-eeprom-trace.jsonl"

set +e
"$PS_EXE" -NoProfile -ExecutionPolicy Bypass -File "$CAP_WIN" \
	-FirmwareBinary "$FW_WIN" \
	-FirmwareSourceRoot "$FWS_WIN" \
	-HostUrl "http://127.0.0.1:5000" \
	-RunSeconds "$SEC" \
	-OutputDir "$(cygpath -aw "$phase1_dir")" \
	-TraceProfile "$COINLINE_TRACE_PROFILE" \
	-VoiceRomU16 "$U16_WIN" \
	-VoiceRomU26 "$U26_WIN" \
	-VoiceRomLayout banked_two_roms \
	-NvramDirectory "$NVP_WIN" \
	-InitialNvramJson "$ZERO_WIN" \
	"${EXTRA_PS_EARLY[@]}" \
	-Screenshot \
	-RealInputDemo
P1=$?
set -e

PH1_NVRAM_PATH=""
PH1_NVRAM_SHA256=""
while IFS= read -r -d '' f; do
	if [[ "$(basename "$f")" == "nvram" ]]; then
		PH1_NVRAM_PATH="$f"
		break
	fi
done < <(find "$NVPERSIST" -type f -print0 2>/dev/null || true)
if [[ -z "$PH1_NVRAM_PATH" ]]; then
	while IFS= read -r -d '' f; do
		PH1_NVRAM_PATH="$f"
		break
	done < <(find "$NVPERSIST" -type f -print0 2>/dev/null || true)
fi
if [[ -n "$PH1_NVRAM_PATH" && -f "$PH1_NVRAM_PATH" ]]; then
	PH1_NVRAM_SHA256=$(sha256sum "$PH1_NVRAM_PATH" | awk '{print $1}')
fi

# --- Phase 2: persisted .nv only ---
unset COINLINE_NVRAM
export COINLINE_NVRAM_STORAGE_TRACE="$(cygpath -w "$phase2_dir/nvram-storage-trace.jsonl")"
export COINLINE_MICROWIRE_TRACE="$(cygpath -w "$phase2_dir/microwire-eeprom-trace.jsonl")"
: >"$phase2_dir/nvram-storage-trace.jsonl"
: >"$phase2_dir/microwire-eeprom-trace.jsonl"

set +e
"$PS_EXE" -NoProfile -ExecutionPolicy Bypass -File "$CAP_WIN" \
	-FirmwareBinary "$FW_WIN" \
	-FirmwareSourceRoot "$FWS_WIN" \
	-HostUrl "http://127.0.0.1:5000" \
	-RunSeconds "$SEC" \
	-OutputDir "$(cygpath -aw "$phase2_dir")" \
	-TraceProfile "$COINLINE_TRACE_PROFILE" \
	-VoiceRomU16 "$U16_WIN" \
	-VoiceRomU26 "$U26_WIN" \
	-VoiceRomLayout banked_two_roms \
	-NvramDirectory "$NVP_WIN" \
	"${EXTRA_PS_EARLY[@]}" \
	-Screenshot \
	-RealInputDemo
P2=$?
set -e

PH2_NVRAM_SHA256=""
PH2_NVRAM_SIZE=""
if [[ -n "$PH1_NVRAM_PATH" && -f "$PH1_NVRAM_PATH" ]]; then
	PH2_NVRAM_SHA256=$(sha256sum "$PH1_NVRAM_PATH" | awk '{print $1}')
	PH2_NVRAM_SIZE=$(wc -c <"$PH1_NVRAM_PATH" | tr -d ' ')
fi

export PHASE1_EXIT="$P1"
export PHASE2_EXIT="$P2"
export COINLINE_TWO_PHASE_PH1_NVRAM_PATH="${PH1_NVRAM_PATH:-}"
export COINLINE_TWO_PHASE_PH1_NVRAM_SHA256="${PH1_NVRAM_SHA256:-}"
export COINLINE_TWO_PHASE_PH2_NVRAM_SHA256="${PH2_NVRAM_SHA256:-}"
export COINLINE_TWO_PHASE_PH2_NVRAM_SIZE="${PH2_NVRAM_SIZE:-}"
python3 - "$RUN" "$P1" "$P2" <<'PY'
import json, os, pathlib, sys
run = pathlib.Path(sys.argv[1])
p1 = int(sys.argv[2])
p2 = int(sys.argv[3])

def scan(path):
    p = pathlib.Path(path)
    if not p.exists():
        return []
    out = []
    for line in p.read_text(encoding="utf-8", errors="replace").splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            out.append(json.loads(line))
        except Exception:
            pass
    return out

def microwire_op_counts(mw):
    c = {}
    for x in mw:
        if x.get("event") != "microwire_eeprom":
            continue
        op = x.get("op") or "unknown"
        c[op] = c.get(op, 0) + 1
    return c

def ewen_before_first_write(mw):
    saw_ewen = False
    for x in mw:
        if x.get("event") != "microwire_eeprom":
            continue
        op = x.get("op")
        if op == "ewen":
            saw_ewen = True
        elif op == "write":
            return saw_ewen
    return False

def summarize(phase_dir, label):
    d = run / phase_dir
    tr = scan(d / "nvram-storage-trace.jsonl")
    mw = scan(d / "microwire-eeprom-trace.jsonl")
    r0 = [x for x in tr if x.get("event") == "nvram_storage" and x.get("rw") == "r" and int(x.get("region_offset", -1)) == 0]
    w0 = [x for x in tr if x.get("event") == "nvram_storage" and x.get("rw") == "w" and int(x.get("region_offset", -1)) == 0]
    mw_writes = [x for x in mw if x.get("event") == "microwire_eeprom" and x.get("op") == "write"]
    mw_reads = [x for x in mw if x.get("event") == "microwire_eeprom" and x.get("op") == "read"]
    mwc = microwire_op_counts(mw)
    return {
        "phase": label,
        "nvram_storage_events": len(tr),
        "reads_offset_0": len(r0),
        "writes_offset_0": len(w0),
        "first_read_offset_0_data": (r0[0].get("data") if r0 else None),
        "microwire_events": len(mw),
        "microwire_write_ops": len(mw_writes),
        "microwire_read_ops": len(mw_reads),
        "microwire_ewen_ops": mwc.get("ewen", 0),
        "microwire_ewds_ops": mwc.get("ewds", 0),
        "microwire_write_rejected_ops": mwc.get("write_rejected", 0),
        "microwire_ewen_before_first_write": ewen_before_first_write(mw),
    }

ph1 = summarize("phase1-first-boot", "first_boot_blank_json")
ph2 = summarize("phase2-persisted-nv", "second_boot_persisted_nv")

def gate_g1(p1s):
    if p1s["microwire_read_ops"] <= 0:
        return False, "G1: phase1 microwire_read_ops==0"
    if p1s["microwire_write_ops"] <= 0:
        return False, "G1: phase1 microwire_write_ops==0"
    if not p1s["microwire_ewen_before_first_write"]:
        return False, "G1: no EWEN before first Microwire write"
    return True, ""

def gate_g3(p1s, p2s):
    if p2s["microwire_read_ops"] <= 0:
        return False, "G3: phase2 microwire_read_ops==0"
    if p2s["microwire_write_ops"] > p1s["microwire_write_ops"]:
        return False, "G3: phase2 writes exceed phase1 (unexpected full re-init)"
    # Heuristic: second boot should not mass-program like first boot.
    if p1s["microwire_write_ops"] > 0 and p2s["microwire_write_ops"] >= p1s["microwire_write_ops"]:
        return False, "G3: phase2 write count not lower than phase1 (possible repeated init)"
    return True, ""

g1_ok, g1_reason = gate_g1(ph1)
g3_ok, g3_reason = gate_g3(ph1, ph2)
if not g1_ok:
    g3_ok = False
    if not g3_reason:
        g3_reason = "G3: cannot assess second boot until G1 first-boot EEPROM programming is proven"
first_fail = ""
if not g1_ok:
    first_fail = g1_reason
elif not g3_ok:
    first_fail = g3_reason

nv_path = os.environ.get("COINLINE_TWO_PHASE_PH1_NVRAM_PATH", "").strip()
summary = {
    "schema_version": "coinline.craft_nvram_two_phase/v2",
    "run_dir": str(run).replace("\\", "/"),
    "phase1_exit": p1,
    "phase2_exit": p2,
    "persistence": {
        "mame_nvram_file": nv_path.replace("\\", "/") if nv_path else None,
        "phase1_file_sha256": os.environ.get("COINLINE_TWO_PHASE_PH1_NVRAM_SHA256") or None,
        "phase2_file_sha256": os.environ.get("COINLINE_TWO_PHASE_PH2_NVRAM_SHA256") or None,
        "phase2_file_size": int(os.environ["COINLINE_TWO_PHASE_PH2_NVRAM_SIZE"]) if os.environ.get("COINLINE_TWO_PHASE_PH2_NVRAM_SIZE") else None,
        "note": "Authoritative persistence for Microwire-backed setup is the MAME nvram_directory device image; nvram-storage-trace.jsonl only reflects logical storage_nvram_* map traffic (often empty when firmware uses 93C66 only).",
    },
    "gates": {
        "G1_pass": g1_ok,
        "G1_first_failure": g1_reason or None,
        "G3_pass": g3_ok,
        "G3_first_failure": g3_reason or None,
        "first_failed_eeprom_gate": first_fail or None,
    },
    "phase1": ph1,
    "phase2": ph2,
}
out = run / "nvram-two-phase-summary.json"
out.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
print(out.read_text(encoding="utf-8"))
PY

echo "OK: $RUN (phase1 exit=$P1 phase2 exit=$P2)"
