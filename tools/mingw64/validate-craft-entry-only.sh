#!/usr/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EMU_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$EMU_ROOT"

RUN_FILE="${EMU_ROOT}/build/runs/latest-craft-entry-only.txt"
if [[ ! -f "$RUN_FILE" ]]; then
	echo "ERROR: latest craft-only run pointer missing: $RUN_FILE"
	exit 2
fi
RUN_DIR="$(tr -d '\r\n' < "$RUN_FILE")"
SUM="${RUN_DIR}/craft-entry-summary.json"
if [[ ! -f "$SUM" ]]; then
	echo "ERROR: craft entry summary missing: $SUM"
	exit 2
fi

python3 - "$SUM" "$RUN_DIR" <<'PY'
import json
import pathlib
import sys

sum_path = pathlib.Path(sys.argv[1])
run_dir = pathlib.Path(sys.argv[2])
data = json.loads(sum_path.read_text(encoding="utf-8"))

required_files = [
    "craft-truth-chain.jsonl",
    "craft-entry-gate-trace.jsonl",
    "tp-cp-keypad-protocol-trace.jsonl",
    "cp-key-consumption-trace.jsonl",
    "display-queue-trace.jsonl",
    "vfd-trace.jsonl",
    "vfd-snapshots.jsonl",
    "vfd-final-text.txt",
    "craft-entry-summary.json",
]
missing_files = [f for f in required_files if not (run_dir / f).exists()]

flags = [
    "a6_oos_stable_before_code",
    "a6_onhook_before_code",
    "a6_tp_ready_before_code",
    "a6_service_input_active_if_required",
    "a6_security_inputs_eligible",
    "a6_tp_reported_2727378",
    "a6_cp_consumed_2727378",
    "a6_cp_buffer_contains_2727378",
    "a6_firmware_craft_parser_entered",
    "a6_craft_gate_accept",
    "a6_display_queue_received_craft_message",
    "a6_vfd_rendered_craft_screen",
]

print(f"run_dir={run_dir}")
for k in flags:
    print(f"{k}={data.get(k, False)}")
print(f"a6_classification={data.get('a6_classification', '')}")
print(f"cp_consumed_sequence={data.get('cp_consumed_sequence', '')}")

errs = []
if missing_files:
    errs.append("missing_artifacts:" + ",".join(missing_files))
if not data.get("a6_oos_stable_before_code", False):
    errs.append("oos_not_stable_before_code")
if not data.get("a6_onhook_before_code", False):
    errs.append("hook_not_onhook_before_code")
if not data.get("a6_tp_reported_2727378", False):
    errs.append("tp_did_not_report_2727378")
if not data.get("a6_cp_consumed_2727378", False):
    errs.append("cp_did_not_consume_2727378")
if not data.get("a6_pass", False):
    errs.append("A6_FAIL")

if errs:
    print("VALIDATION_FAIL: " + ",".join(errs))
    sys.exit(1)
print("VALIDATION_OK")
PY

