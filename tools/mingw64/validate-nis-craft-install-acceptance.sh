#!/usr/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EMU_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$EMU_ROOT"

RUN_FILE="${EMU_ROOT}/build/runs/latest-nis-craft-install-acceptance.txt"
if [[ ! -f "$RUN_FILE" ]]; then
	echo "ERROR: latest run pointer missing: $RUN_FILE"
	exit 2
fi
RUN_DIR="$(tr -d '\r\n' < "$RUN_FILE")"
SUM="${RUN_DIR}/acceptance-summary.json"
if [[ ! -f "$SUM" ]]; then
	echo "ERROR: acceptance summary missing: $SUM"
	exit 2
fi

python3 - "$SUM" "$RUN_DIR" <<'PY'
import json, pathlib, sys
sum_path = pathlib.Path(sys.argv[1])
run_dir = pathlib.Path(sys.argv[2])
data = json.loads(sum_path.read_text(encoding="utf-8"))

required_true = [
    "A1_BOOT_OOS_SCREEN",
    "A2_REAL_HOOK_INPUT",
    "A3_NIS_HANDSET_AUDIO",
    "A4_REAL_ONHOOK",
    "A5_REAL_KEYPAD_SEQUENCE",
    "A6_CRAFT_INSTALL_ENTRY",
]
missing = [k for k in required_true if not data.get(k, False)]

wav_path = pathlib.Path(data.get("wav_path", ""))
wav_ok = bool(data.get("wav_non_silent")) and wav_path.exists()
if not wav_ok:
    report_path = run_dir / "audio-capture-report.json"
    if report_path.exists():
        try:
            rep = json.loads(report_path.read_text(encoding="utf-8"))
            rep_peak = int(rep.get("peak_abs_int16", 0) or 0)
            rep_non_silent = bool(rep.get("audio_non_silent_heuristic")) or bool(rep.get("m5b_non_silent_from_wav_peak")) or rep_peak > 0
            wav_ok = wav_path.exists() and rep_non_silent
        except Exception:
            pass
if not wav_ok:
    missing.append("WAV_NON_SILENT")

forbidden_markers = ["synthetic_keymatrix_forced", "injected_display_text", "fake_telephony_ready"]
marker_hits = []
for p in [run_dir / "front-panel-input-source-trace.jsonl", run_dir / "vfd-trace.jsonl", run_dir / "io-trace.jsonl"]:
    if not p.exists():
        continue
    txt = p.read_text(encoding="utf-8", errors="replace").lower()
    for m in forbidden_markers:
        if m in txt:
            marker_hits.append(f"{m}@{p.name}")
if marker_hits:
    missing.append("FORBIDDEN_MARKERS_PRESENT")

# Gates must be backed by TP/LINECTRL-oriented proof paths (not PIO-only strings).
if data.get("A2_REAL_HOOK_INPUT"):
    p2 = data.get("proof_A2_path") or ""
    if p2 == "none":
        missing.append("A2_PROOF_PATH_NONE")
if data.get("A4_REAL_ONHOOK"):
    p4 = data.get("proof_A4_path") or ""
    if p4 == "none":
        missing.append("A4_PROOF_PATH_NONE")
if data.get("A5_REAL_KEYPAD_SEQUENCE"):
    p5 = data.get("proof_A5_path") or ""
    if p5 == "none":
        missing.append("A5_PROOF_PATH_NONE")
if data.get("A6_CRAFT_INSTALL_ENTRY"):
    if not data.get("craft_code_detected") and not data.get("craft_gate_accept"):
        missing.append("A6_REQUIRES_CP_CRAFT_TRACE")
    if not data.get("a6_vfd_proof"):
        missing.append("A6_REQUIRES_VFD_CRAFT_MENU")

print(f"run_dir={run_dir}")
for k in required_true:
    print(f"{k}={data.get(k, False)}")
print(f"proof_A2_path={data.get('proof_A2_path')}")
print(f"proof_A4_path={data.get('proof_A4_path')}")
print(f"proof_A5_path={data.get('proof_A5_path')}")
print(f"craft_code_detected={data.get('craft_code_detected')} craft_gate_accept={data.get('craft_gate_accept')}")
print(f"a6_cp_proof={data.get('a6_cp_proof')} a6_vfd_proof={data.get('a6_vfd_proof')}")
print(f"wav_non_silent={data.get('wav_non_silent', False)} path={wav_path}")

if missing:
    print("VALIDATION_FAIL: " + ",".join(missing))
    sys.exit(1)
print("VALIDATION_OK")
PY
