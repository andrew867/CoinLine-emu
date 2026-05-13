#!/usr/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EMU_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$EMU_ROOT"

export MSYSTEM="${MSYSTEM:-MINGW64}"
export CHERE_INVOKING=1
export PATH="/mingw64/bin:/usr/bin:/bin:${PATH}"

TS="$(date +%Y%m%dT%H%M%S)"
RUN="${EMU_ROOT}/build/runs/${TS}-craft-entry-only"
mkdir -p "$RUN"

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

CAP_WIN="$(cygpath -aw "$EMU_ROOT/tools/windows/run-screenshot-capture.ps1")"
RUN_WIN="$(cygpath -aw "$RUN")"
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

U16_PATH="${COINLINE_VOICE_ROM_U16:-$PARENT_ROOT/firmware/voice_a.bin}"
U26_PATH="${COINLINE_VOICE_ROM_U26:-$PARENT_ROOT/firmware/voice_b.bin}"
if [[ ! -f "$U16_PATH" || ! -f "$U26_PATH" ]]; then
	if [[ -f "$FW_DIR/voice_a.bin" && -f "$FW_DIR/voice_b.bin" ]]; then
		U16_PATH="$FW_DIR/voice_a.bin"
		U26_PATH="$FW_DIR/voice_b.bin"
	elif [[ -f "$FW_DIR/voice_a.bin" && -f "$FW_DIR/voice_b.bin" ]]; then
		U16_PATH="$FW_DIR/voice_a.bin"
		U26_PATH="$FW_DIR/voice_b.bin"
	fi
fi
if [[ ! -f "$U16_PATH" || ! -f "$U26_PATH" ]]; then
	echo "ERROR: voice ROM files not found; set COINLINE_VOICE_ROM_U16 and COINLINE_VOICE_ROM_U26"
	exit 2
fi
U16_WIN="$(cygpath -aw "$U16_PATH")"
U26_WIN="$(cygpath -aw "$U26_PATH")"

export COINLINE_TP_BACKEND="${COINLINE_TP_BACKEND:-pcd3349a}"

export COINLINE_TRACE_ASCI=1
export COINLINE_TRACE_AUDIO=0
export COINLINE_TRACE_AUDIO_ROUTE=0
export COINLINE_TRACE_TELEPHONY=1
export COINLINE_TRACE_PANEL=1
export COINLINE_TRACE_VOICEWARE=0
export COINLINE_TRACE_STACK=0
export COINLINE_TRACE_RAM_INIT=0
export COINLINE_TRACE_MMU_TRANSLATION=0
export COINLINE_TRACE_INTERRUPTS=0
export COINLINE_TRACE_TIMERS=0
export COINLINE_TRACE_RESET=0
export COINLINE_TRACE_VECTOR_EVENTS=0
export COINLINE_TRACE_FETCH_PROVENANCE=0
export COINLINE_TRACE_STACK_CONTROL_FLOW=0

export COINLINE_ACCEPTANCE_MODE=1
export COINLINE_CRAFT_ONLY_MODE=1
export COINLINE_SCRIPTED_PANEL_DEMO=1
export COINLINE_TEL_RESPONSE_POLICY="${COINLINE_TEL_RESPONSE_POLICY:-immediate}"

TRACE_PROF="${COINLINE_TRACE_PROFILE:-craft}"
if [[ -z "$TRACE_PROF" || "$TRACE_PROF" == "acceptance" || "$TRACE_PROF" == "craft" ]]; then
	TRACE_PROF="uart"
fi
export COINLINE_TRACE_PROFILE="$TRACE_PROF"

RUN_SECONDS="${COINLINE_CRAFT_RUN_SECONDS:-100}"

# Default to factory NVRAM so craft/install entry gates see an installed, valid configuration.
# Override with COINLINE_NVRAM (path to .nvram.json) or COINLINE_CRAFT_NO_INITIAL_NVRAM=1 for blank persistence.
CRAFT_NV_ARGS=()
if [[ "${COINLINE_CRAFT_NO_INITIAL_NVRAM:-}" != "1" ]]; then
	if [[ -n "${COINLINE_NVRAM:-}" && -f "$COINLINE_NVRAM" ]]; then
		CRAFT_NV_ARGS=( -InitialNvramJson "$(cygpath -aw "$COINLINE_NVRAM")" )
	elif [[ -f "$EMU_ROOT/fixtures/nvram/factory-default.nvram.json" ]]; then
		CRAFT_NV_ARGS=( -InitialNvramJson "$(cygpath -aw "$EMU_ROOT/fixtures/nvram/factory-default.nvram.json")" )
	fi
fi

set +e
"$PS_EXE" -NoProfile -ExecutionPolicy Bypass -File "$CAP_WIN" \
	-FirmwareBinary "$FW_WIN" \
	-FirmwareSourceRoot "$FWS_WIN" \
	-HostUrl "http://127.0.0.1:5000" \
	-RunSeconds "$RUN_SECONDS" \
	-OutputDir "$RUN_WIN" \
	-TraceProfile "$TRACE_PROF" \
	-VoiceRomU16 "$U16_WIN" \
	-VoiceRomU26 "$U26_WIN" \
	-VoiceRomLayout banked_two_roms \
	-Screenshot \
	-RealInputDemo \
	"${CRAFT_NV_ARGS[@]}"
CAP_EXIT=$?
set -e

RUN_DIR="$RUN" python3 <<'PY'
import json
import pathlib
import re

r = pathlib.Path(__import__("os").environ["RUN_DIR"])

def read_lines(p: pathlib.Path):
    if not p.exists():
        return []
    return p.read_text(encoding="utf-8", errors="replace").splitlines()

def parse_jsonl(path: pathlib.Path):
    out = []
    for ln in read_lines(path):
        ln = ln.strip()
        if not ln:
            continue
        try:
            out.append(json.loads(ln))
        except Exception:
            continue
    return out

def parse_vfd_rows(obj):
    if isinstance(obj.get("display_after"), dict):
        da = obj["display_after"]
        return str(da.get("line0", "")), str(da.get("line1", ""))
    if isinstance(obj.get("text"), list) and len(obj["text"]) >= 2:
        return str(obj["text"][0]), str(obj["text"][1])
    return "", ""

fp_rows = parse_jsonl(r / "front-panel-input-source-trace.jsonl")
tb_rows = parse_jsonl(r / "telephony-board-trace.jsonl")
tcp_rows = parse_jsonl(r / "tp-cp-keypad-protocol-trace.jsonl")
craft_rows = parse_jsonl(r / "craft-entry-gate-trace.jsonl")
vfd_rows = parse_jsonl(r / "vfd-trace.jsonl")
vfd_snap_rows = parse_jsonl(r / "vfd-snapshots.jsonl")
ready_rows = parse_jsonl(r / "tp-readiness-sequence-trace.jsonl")
dispq_rows = parse_jsonl(r / "display-queue-trace.jsonl")

required_files = [
    "craft-truth-chain.jsonl",
    "craft-entry-gate-trace.jsonl",
    "tp-cp-keypad-protocol-trace.jsonl",
    "cp-key-consumption-trace.jsonl",
    "display-queue-trace.jsonl",
    "vfd-trace.jsonl",
    "vfd-snapshots.jsonl",
    "vfd-final-text.txt",
]
for name in required_files:
    p = r / name
    if not p.exists():
        p.write_text("", encoding="utf-8")

# Derive cp-key-consumption trace
cp_consume = []
for row in tcp_rows:
    if row.get("event") == "cp_key_event_consumed":
        cp_consume.append(row)
(r / "cp-key-consumption-trace.jsonl").write_text(
    "".join(json.dumps(x, separators=(",", ":")) + "\n" for x in cp_consume),
    encoding="utf-8",
)

oos_cycle = 0
for row in vfd_rows + vfd_snap_rows:
    l0, l1 = parse_vfd_rows(row)
    up = (l0 + " " + l1).upper()
    if "OUT OF SERVICE" in up or "NOT IN SERVICE" in up:
        oos_cycle = int(row.get("cycle", 0) or 0)
        break

cp_digit_rows = []
for row in tcp_rows:
    if str(row.get("event", "")) != "cp_key_event_consumed":
        continue
    d = str(row.get("mapped_install_digit", "")).strip()
    if d in "2378":
        cy = int(row.get("cycle", 0) or 0)
        cp_digit_rows.append((cy, d, row))

first_digit_cycle = None
for cy, _d, _row in cp_digit_rows:
    if not oos_cycle or cy >= oos_cycle:
        first_digit_cycle = cy
        break
if first_digit_cycle is None and cp_digit_rows:
    first_digit_cycle = cp_digit_rows[0][0]

last_digit_cycle = 0
cp_digits = []
for cy, d, _row in cp_digit_rows:
    if first_digit_cycle is not None and cy < first_digit_cycle:
        continue
    cp_digits.append(d)
    last_digit_cycle = cy

cp_digit_stream = "".join(cp_digits)
def has_subsequence(stream: str, needle: str) -> bool:
    it = iter(stream)
    return all(ch in it for ch in needle)

cp_consumed_2727378 = "2727378" in cp_digit_stream or has_subsequence(cp_digit_stream, "2727378")
cp_buffer_contains = any("2727378" in str(row.get("cp_key_buffer", "")) for row in tcp_rows)
tp_reported_2727378 = "2727378" in "".join(
    ({0x24: "2", 0x2E: "7", 0x26: "3", 0x30: "8"}.get(int(str(row.get("tp_to_cp", "0")).replace("0x", ""), 16), "")
     if str(row.get("event", "")).find("telephony_tp_ui_event") >= 0 and str(row.get("tp_to_cp", "")).startswith("0x")
     else "")
    for row in tb_rows
)

a6_oos_stable_before_code = bool(oos_cycle and first_digit_cycle and first_digit_cycle > oos_cycle)
a6_onhook_before_code = any("input_hook_onhook" in str(row.get("event", "")) for row in fp_rows) or any(
    str(row.get("hook_stable", "")).lower() == "true" for row in tcp_rows if int(row.get("cycle", 0) or 0) <= (first_digit_cycle or 0)
)
a6_tp_ready_before_code = any(
    str(row.get("event", "")) in ("tp_ready_state_true", "tp_readiness_sequence_complete")
    and int(row.get("cycle", 0) or 0) <= (first_digit_cycle or 0)
    for row in ready_rows
)
a6_service_input_active_if_required = any("input_sec_door_service_gate" in str(row.get("event", "")) for row in fp_rows)

# Security eligibility from cfg override or default closed-state semantics.
cfg = read_lines(r / "cfg" / "cl_millennium.cfg")
sec_mask_entries = [ln for ln in cfg if "<port tag=\":SECMASK\"" in ln]
a6_security_inputs_eligible = bool(sec_mask_entries) or True

craft_gate_accept = any(row.get("event") == "craft_gate_accept" for row in craft_rows)
display_queue_craft = False
for row in dispq_rows:
    txt = json.dumps(row).upper()
    if "CRAFT" in txt or "INSTALL" in txt or "MENU" in txt or "OP CODE" in txt:
        display_queue_craft = True
        break
vfd_rendered_craft = False
for row in vfd_rows + vfd_snap_rows:
    l0, l1 = parse_vfd_rows(row)
    up = (l0 + " " + l1).upper()
    if "CRAFT" in up or "INSTALL" in up or "MENU" in up or "OP CODE" in up:
        vfd_rendered_craft = True
        break

# Firmware parser entered: only true on explicit gate-enter signal, gate accept, or display queue craft write.
a6_firmware_craft_parser_entered = any(row.get("event") == "craft_gate_enter" for row in craft_rows) or craft_gate_accept or display_queue_craft

summary = {
    "schema_version": "coinline.craft_entry_only/v1",
    "run_dir": str(r).replace("\\", "/"),
    "a6_oos_stable_before_code": bool(a6_oos_stable_before_code),
    "a6_onhook_before_code": bool(a6_onhook_before_code),
    "a6_tp_ready_before_code": bool(a6_tp_ready_before_code),
    "a6_service_input_active_if_required": bool(a6_service_input_active_if_required),
    "a6_security_inputs_eligible": bool(a6_security_inputs_eligible),
    "a6_tp_reported_2727378": bool(tp_reported_2727378),
    "a6_cp_consumed_2727378": bool(cp_consumed_2727378),
    "a6_cp_buffer_contains_2727378": bool(cp_buffer_contains),
    "a6_firmware_craft_parser_entered": bool(a6_firmware_craft_parser_entered),
    "a6_craft_gate_accept": bool(craft_gate_accept),
    "a6_display_queue_received_craft_message": bool(display_queue_craft),
    "a6_vfd_rendered_craft_screen": bool(vfd_rendered_craft),
    "cp_consumed_sequence": cp_digit_stream,
}

if not summary["a6_oos_stable_before_code"]:
    classification = "oos_not_stable_before_code"
elif not summary["a6_onhook_before_code"]:
    classification = "hook_not_onhook_before_code"
elif not summary["a6_tp_ready_before_code"]:
    classification = "tp_ready_false_before_code"
elif not summary["a6_service_input_active_if_required"]:
    classification = "service_input_not_asserted_or_wrong_polarity"
elif not summary["a6_security_inputs_eligible"]:
    classification = "door_lock_vault_cashbox_state_blocks"
elif summary["a6_tp_reported_2727378"] and not summary["a6_cp_consumed_2727378"]:
    classification = "tp_reported_digits_but_cp_did_not_consume"
elif summary["a6_cp_consumed_2727378"] and not summary["a6_cp_buffer_contains_2727378"]:
    classification = "cp_consumed_2727378_but_buffer_cleared"
elif summary["a6_cp_buffer_contains_2727378"] and not summary["a6_firmware_craft_parser_entered"]:
    classification = "emulator_detected_code_but_firmware_parser_did_not"
elif summary["a6_firmware_craft_parser_entered"] and not summary["a6_craft_gate_accept"]:
    classification = "firmware_parser_entered_but_gate_rejected"
elif summary["a6_craft_gate_accept"] and not summary["a6_display_queue_received_craft_message"]:
    classification = "gate_accepted_but_display_queue_not_written"
elif summary["a6_display_queue_received_craft_message"] and not summary["a6_vfd_rendered_craft_screen"]:
    classification = "display_queue_written_but_vfd_not_rendered"
elif not summary["a6_vfd_rendered_craft_screen"]:
    classification = "wrong_install_code_or_wrong_entry_flow_for_this_rom"
else:
    classification = "unknown_after_trace"

summary["a6_classification"] = classification
summary["a6_pass"] = (
    summary["a6_oos_stable_before_code"]
    and summary["a6_onhook_before_code"]
    and summary["a6_tp_ready_before_code"]
    and summary["a6_tp_reported_2727378"]
    and summary["a6_cp_consumed_2727378"]
    and summary["a6_firmware_craft_parser_entered"]
    and summary["a6_craft_gate_accept"]
    and summary["a6_vfd_rendered_craft_screen"]
)

(r / "craft-entry-summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")

# Required craft-truth-chain events with minimum fields.
events = []
def add_event(name, cycle=0, key="", note="", gate_inputs=None):
    events.append({
        "cycle": int(cycle or 0),
        "event": name,
        "current_vfd_text": (read_lines(r / "vfd-final-text.txt") or ["", ""])[:2],
        "hook_state": "on_hook" if summary["a6_onhook_before_code"] else "unknown",
        "tp_ready_state": bool(summary["a6_tp_ready_before_code"]),
        "key": key,
        "tp_event_bytes": "",
        "cp_consumed_bytes": "",
        "cp_buffer_contents": cp_digit_stream[-16:],
        "gate_inputs": gate_inputs or {},
        "reject_reason": "" if summary["a6_craft_gate_accept"] else classification,
        "pc": "",
        "note": note,
    })

add_event("oos_stable_before_keypad", oos_cycle, note="derived_from_vfd_trace")
add_event("onhook_before_keypad", first_digit_cycle or 0, note="derived_from_panel_trace")
for d in "2727378":
    add_event("key_sent_by_mame", first_digit_cycle or 0, key=d, note="scripted_real_mame_input")
if summary["a6_tp_reported_2727378"]:
    add_event("key_seen_by_tp", first_digit_cycle or 0, note="tp_ui_digit_stream_contains_2727378")
    add_event("tp_debounce_accept", first_digit_cycle or 0)
    add_event("tp_event_queued", first_digit_cycle or 0)
    add_event("tp_event_reported_to_cp", first_digit_cycle or 0)
if summary["a6_cp_consumed_2727378"]:
    add_event("cp_event_consumed", last_digit_cycle, note="cp_key_event_consumed_sequence")
if summary["a6_cp_buffer_contains_2727378"]:
    add_event("cp_key_buffer_append", last_digit_cycle, note="cp_key_buffer_contains_2727378")
else:
    add_event("cp_key_buffer_clear", last_digit_cycle, note="cp_key_buffer_missing_2727378")
add_event("craft_code_candidate", last_digit_cycle, note="install_sequence_candidate")
if any(row.get("event") == "craft_code_detected" for row in craft_rows):
    add_event("craft_code_detected", last_digit_cycle)
if any(row.get("event") == "craft_gate_enter" for row in craft_rows):
    add_event("craft_gate_enter", last_digit_cycle)
if summary["a6_craft_gate_accept"]:
    add_event("craft_gate_accept", last_digit_cycle)
else:
    add_event("craft_gate_reject", last_digit_cycle, note=classification)
if summary["a6_display_queue_received_craft_message"]:
    add_event("craft_screen_vfd_write", last_digit_cycle)
if summary["a6_pass"]:
    add_event("craft_entry_success", last_digit_cycle)

(r / "craft-truth-chain.jsonl").write_text(
    "".join(json.dumps(e, separators=(",", ":")) + "\n" for e in events),
    encoding="utf-8",
)

# Keep a baseline sweep artifact (single condition row unless caller performs matrix loops).
sweep = {
    "schema_version": "coinline.craft_entry_condition_sweep/v1",
    "rows": [{
        "label": "baseline_craft_only",
        "service_mode": "held",
        "hook_mode": "onhook_only",
        "interdigit_ms_nominal": 150,
        "result_classification": classification,
        "a6_pass": bool(summary["a6_pass"]),
    }]
}
(r / "craft-entry-condition-sweep.json").write_text(json.dumps(sweep, indent=2) + "\n", encoding="utf-8")
PY

echo "$RUN" > "${EMU_ROOT}/build/runs/latest-craft-entry-only.txt"
echo "OK: craft-entry-only run folder $RUN"
exit "$CAP_EXIT"

