#!/usr/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Read build/runs/latest-boot-critical.txt; check for M6 + vfd_data @ 0x60 in io-trace.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EMU_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
LATEST="${EMU_ROOT}/build/runs/latest-boot-critical.txt"
LOG="${EMU_ROOT}/build/logs/validate-latest-boot.log"
mkdir -p "$(dirname "$LOG")"

if [[ ! -f "$LATEST" ]]; then
	echo "No ${LATEST}" | tee "$LOG"
	exit 1
fi
RUN_DIR="$(tr -d '\r\n' < "$LATEST")"
{
	echo "=== validate-latest-boot $(date -u +%Y-%m-%dT%H:%M:%SZ) ==="
	echo "run_dir=$RUN_DIR"
} | tee "$LOG"

if [[ ! -d "$RUN_DIR" ]]; then
	echo "Run dir missing" | tee -a "$LOG"
	exit 1
fi

M6=0
VFD60=0
VFD_TRACE=0
if [[ -f "$RUN_DIR/boot-trace.jsonl" || -f "$RUN_DIR/io-trace.jsonl" || -f "$RUN_DIR/vfd-trace.jsonl" ]]; then
	eval "$(RUN_DIR="$RUN_DIR" python3 <<'PY'
import json, os, pathlib
r = pathlib.Path(os.environ["RUN_DIR"])
def has_m6():
    p = r / "boot-trace.jsonl"
    if not p.is_file():
        return False
    for ln in p.read_text(encoding="utf-8", errors="ignore").splitlines():
        ln = ln.strip()
        if not ln:
            continue
        try:
            if json.loads(ln).get("milestone") == "M6":
                return True
        except Exception:
            continue
    return False
def has_vfd60():
    p = r / "io-trace.jsonl"
    if not p.is_file():
        return False
    for ln in p.read_text(encoding="utf-8", errors="ignore").splitlines():
        ln = ln.strip()
        if not ln:
            continue
        try:
            row = json.loads(ln)
        except Exception:
            continue
        if row.get("port") == "0x0060" and row.get("rw") == "w" and row.get("tag") == "vfd_data":
            return True
    return False
def has_vfd_trace():
    p = r / "vfd-trace.jsonl"
    if not p.is_file():
        return False
    for ln in p.read_text(encoding="utf-8", errors="ignore").splitlines():
        ln = ln.strip()
        if not ln:
            continue
        try:
            row = json.loads(ln)
        except Exception:
            continue
        if row.get("port") == "0x0060" or row.get("tag") in ("vfd_data", "vfd_status"):
            return True
    return False
def has_milestone(tag):
    p = r / "boot-trace.jsonl"
    if not p.is_file():
        return False
    for ln in p.read_text(encoding="utf-8", errors="ignore").splitlines():
        ln = ln.strip()
        if not ln:
            continue
        try:
            if json.loads(ln).get("milestone") == tag:
                return True
        except Exception:
            continue
    return False
print(f"M6={'1' if has_m6() else '0'}")
print(f"VFD60={'1' if has_vfd60() else '0'}")
print(f"VFD_TRACE={'1' if has_vfd_trace() else '0'}")
print(f"M7A={'1' if has_milestone('M7A') else '0'}")
print(f"M7B={'1' if has_milestone('M7B') else '0'}")
print(f"M7C={'1' if has_milestone('M7C') else '0'}")
PY
)"
fi

{
	echo "m6_in_boot_trace=$M6"
	echo "m7a_in_boot_trace=${M7A:-0}"
	echo "m7b_in_boot_trace=${M7B:-0}"
	echo "m7c_in_boot_trace=${M7C:-0}"
	echo "vfd_line_in_io_trace=$VFD60"
	echo "vfd_trace_present=$VFD_TRACE"
} | tee -a "$LOG"

if [[ $M6 -eq 1 && $VFD60 -eq 1 && $VFD_TRACE -eq 1 ]]; then
	echo "OK: M6 present and io-trace has VFD activity" | tee -a "$LOG"
	exit 0
fi
echo "FAIL: strict M6 check (need M6 milestone + firmware 0x0060 write + vfd-trace evidence)" | tee -a "$LOG"
exit 1
