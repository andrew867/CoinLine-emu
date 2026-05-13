#!/usr/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Validate artifacts from run-craft-nvram-two-phase.sh (G1/G3 + persistence paths).
# Usage: validate-craft-nvram-two-phase.sh [run_dir]
# Default: latest build/runs/*-craft-nvram-two-phase by mtime.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EMU_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$EMU_ROOT"

RUN="${1:-}"
if [[ -z "$RUN" ]]; then
	RUN="$(find "$EMU_ROOT/build/runs" -maxdepth 1 -type d -name '*-craft-nvram-two-phase' -printf '%T@ %p\n' 2>/dev/null | sort -nr | head -1 | cut -d' ' -f2-)"
fi
if [[ -z "$RUN" || ! -d "$RUN" ]]; then
	echo "ERROR: no run directory (pass path or create a two-phase run under build/runs/)"
	exit 2
fi

SUM="$RUN/nvram-two-phase-summary.json"
if [[ ! -f "$SUM" ]]; then
	echo "ERROR: missing $SUM"
	exit 3
fi

python3 - "$RUN" "$SUM" <<'PY'
import json, pathlib, sys

run = pathlib.Path(sys.argv[1])
summ_path = pathlib.Path(sys.argv[2])
doc = json.loads(summ_path.read_text(encoding="utf-8"))
ph1 = run / "phase1-first-boot"
ph2 = run / "phase2-persisted-nv"
pers = doc.get("persistence") or {}
gates = doc.get("gates") or {}

def lines(p):
    if not p.exists():
        return 0
    return sum(1 for line in p.read_text(encoding="utf-8", errors="replace").splitlines() if line.strip())

def head_note(path, n=2):
    if not path.exists():
        return "(missing)"
    ls = [ln for ln in path.read_text(encoding="utf-8", errors="replace").splitlines() if ln.strip()]
    return " | ".join(ls[:n])

print("validate-craft-nvram-two-phase")
print("  run_dir:", run)
print("  phase1_dir:", ph1)
print("  phase2_dir:", ph2)
for label, p in ("phase1", ph1), ("phase2", ph2):
    mw = p / "microwire-eeprom-trace.jsonl"
    nv = p / "nvram-storage-trace.jsonl"
    print(f"  {label} microwire_jsonl_lines:", lines(mw))
    print(f"  {label} nvram_storage_jsonl_lines:", lines(nv))

p1 = doc.get("phase1") or {}
p2 = doc.get("phase2") or {}
print("  phase1 microwire_read_ops:", p1.get("microwire_read_ops"))
print("  phase1 microwire_ewen_ops:", p1.get("microwire_ewen_ops"))
print("  phase1 microwire_write_ops:", p1.get("microwire_write_ops"))
print("  phase1 microwire_ewds_ops:", p1.get("microwire_ewds_ops"))
print("  phase1 microwire_write_rejected_ops:", p1.get("microwire_write_rejected_ops"))
print("  phase1 ewen_before_first_write:", p1.get("microwire_ewen_before_first_write"))
print("  phase2 microwire_read_ops:", p2.get("microwire_read_ops"))
print("  phase2 microwire_write_ops:", p2.get("microwire_write_ops"))
print("  phase2 repeated_full_init_heuristic:", (p2.get("microwire_write_ops") or 0) >= (p1.get("microwire_write_ops") or 0) and (p1.get("microwire_write_ops") or 0) > 0)

print("  persisted_file_path:", pers.get("mame_nvram_file"))
print("  persisted_file_size:", pers.get("phase2_file_size"))
print("  phase1_persisted_sha256:", pers.get("phase1_file_sha256"))
print("  phase2_persisted_sha256:", pers.get("phase2_file_sha256"))
print("  nvram_storage_trace_status:", "empty_in_both_phases" if (p1.get("nvram_storage_events") == 0 and p2.get("nvram_storage_events") == 0) else "has_events")

g1p = bool(gates.get("G1_pass"))
g3p = bool(gates.get("G3_pass")) and g1p
print("  G1_pass:", g1p)
print("  G3_pass:", g3p, "(G3 requires G1: second boot only meaningful after first-boot writes proven)")
if not g1p:
    print("  G3_effective_failure:", "G3: blocked until G1 first-boot EEPROM programming is proven")
print("  first_failed_eeprom_gate:", gates.get("first_failed_eeprom_gate"))
print("  microwire_sample_phase1:", head_note(ph1 / "microwire-eeprom-trace.jsonl"))
print("  summary_schema:", doc.get("schema_version"))
PY
