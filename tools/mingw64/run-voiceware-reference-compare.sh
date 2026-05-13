#!/usr/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EMU_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$EMU_ROOT"

export MSYSTEM="${MSYSTEM:-MINGW64}"
export CHERE_INVOKING=1
export PATH="/mingw64/bin:/usr/bin:/bin:${PATH}"

RUN_FILE="${EMU_ROOT}/build/runs/latest-nis-craft-install-acceptance.txt"
if [[ ! -f "$RUN_FILE" ]]; then
	echo "ERROR: latest run pointer missing: $RUN_FILE"
	exit 2
fi
RUN_DIR="$(tr -d '\r\n' < "$RUN_FILE")"

LATEST_BOOT="${EMU_ROOT}/build/runs/latest-boot-critical.txt"
U16="${COINLINE_VOICE_ROM_U16:-}"
U26="${COINLINE_VOICE_ROM_U26:-}"
if [[ -z "$U16" || -z "$U26" ]]; then
	if [[ -f "$RUN_DIR/mame-rompath/cl_millennium/voice_a.bin" && -f "$RUN_DIR/mame-rompath/cl_millennium/voice_b.bin" ]]; then
		U16="$RUN_DIR/mame-rompath/cl_millennium/voice_a.bin"
		U26="$RUN_DIR/mame-rompath/cl_millennium/voice_b.bin"
	elif [[ -f "$LATEST_BOOT" ]]; then
		BR="$(tr -d '\r\n' < "$LATEST_BOOT")"
		U16="$BR/mame-rompath/cl_millennium/voice_a.bin"
		U26="$BR/mame-rompath/cl_millennium/voice_b.bin"
	fi
fi
if [[ ! -f "$U16" || ! -f "$U26" ]]; then
	echo "ERROR: voice ROMs not found."
	exit 2
fi

REF_C="$EMU_ROOT/build/tools/nortel-voiceware-decoder.c"
REF_EXE="$EMU_ROOT/build/tools/nortel-voiceware-decoder.exe"
REF_URL="https://raw.githubusercontent.com/hharte/nortel-voiceware-decoder/refs/heads/main/nortel-voiceware-decoder.c"
mkdir -p "$EMU_ROOT/build/tools"
curl -fsSL "$REF_URL" -o "$REF_C"
gcc -O2 -Wall -Wextra -std=c11 "$REF_C" -o "$REF_EXE"

"$REF_EXE" -l "$U16" > "$RUN_DIR/reference-u16-list.txt" 2> "$RUN_DIR/reference-u16-list.stderr.log" || true
"$REF_EXE" -l "$U26" > "$RUN_DIR/reference-u26-list.txt" 2> "$RUN_DIR/reference-u26-list.stderr.log" || true

export RUN_DIR U16 U26 REF_EXE
python3 - <<'PY'
import json, os, pathlib, subprocess, wave, contextlib

run_dir = pathlib.Path(os.environ["RUN_DIR"])
u16 = pathlib.Path(os.environ["U16"])
u26 = pathlib.Path(os.environ["U26"])
ref_exe = pathlib.Path(os.environ["REF_EXE"])

addr_path = run_dir / "voiceware-address-forensics.json"
decode_path = run_dir / "voiceware-decode-trace.jsonl"

addr = json.loads(addr_path.read_text(encoding="utf-8")) if addr_path.exists() else {"entries": []}

def parse_decode(path):
    rows = []
    if not path.exists():
        return rows
    for ln in path.read_text(encoding="utf-8", errors="ignore").splitlines():
        ln = ln.strip()
        if not ln:
            continue
        try:
            rows.append(json.loads(ln))
        except Exception:
            continue
    return rows

decode_rows = parse_decode(decode_path)

def block_count_for_phrase(phrase_hex):
    return len([r for r in decode_rows if r.get("event") == "block_start" and r.get("phrase") == phrase_hex])

def first_ops_for_phrase(phrase_hex):
    ops = []
    for r in decode_rows:
        if r.get("event") == "block_start" and r.get("phrase") == phrase_hex:
            ops.append(r.get("raw_control", ""))
        if len(ops) >= 8:
            break
    return ops

def duration_of_wav(path):
    if not path.exists():
        return 0.0
    try:
        with contextlib.closing(wave.open(str(path), "rb")) as w:
            n = w.getnframes()
            rate = w.getframerate()
            return (float(n) / float(rate)) if rate else 0.0
    except Exception:
        return 0.0

rows = []
for i, e in enumerate(addr.get("entries", [])):
    rom = u26 if e.get("selected_rom") == "U26" else u16
    seg = int(e.get("segment_index", 0))
    msg = int(e.get("phrase_index_within_segment", 0))
    phrase = e.get("port_0x61_phrase_code", "0x00")
    out_wav = run_dir / f"reference-compare-{i}.wav"

    # Determine absolute message index for reference decoder by walking segment tables.
    rom_bytes = rom.read_bytes()
    seg_size = 0x20000
    abs_idx = 0
    for s in range(seg):
        base = s * seg_size
        if base + 1 > len(rom_bytes):
            break
        abs_idx += int(rom_bytes[base]) + 1
    abs_idx += msg

    tmp_dir = run_dir / f"reference-compare-{i}"
    tmp_dir.mkdir(exist_ok=True)
    cp = subprocess.run([str(ref_exe), "-i", str(abs_idx), str(rom)], cwd=tmp_dir, capture_output=True, text=True)
    wavs = sorted(tmp_dir.glob("*.wav"))
    ref_wav = wavs[-1] if wavs else None
    if ref_wav and ref_wav.exists():
        out_wav.write_bytes(ref_wav.read_bytes())
    duration = duration_of_wav(out_wav if out_wav.exists() else pathlib.Path(""))
    rows.append({
        "selected_rom": e.get("selected_rom"),
        "segment_index": seg,
        "message_index": msg,
        "message_offset": e.get("computed_message_offset", ""),
        "message_mode": e.get("message_mode_byte", ""),
        "reference_absolute_message_index": abs_idx,
        "reference_decode_exit_code": cp.returncode,
        "duration_seconds": duration,
        "block_count": block_count_for_phrase(phrase),
        "first_opcode_sequence": first_ops_for_phrase(phrase),
        "offset_match": e.get("message_start_parses_sanely", False),
    })

doc = {
    "schema_version": "coinline.voiceware_reference_compare/v1",
    "reference_decoder": "hharte/nortel-voiceware-decoder.c",
    "comparisons": rows
}
(run_dir / "voiceware-reference-compare.json").write_text(json.dumps(doc, indent=2) + "\n", encoding="utf-8")
PY

echo "OK: voiceware reference compare ${RUN_DIR}/voiceware-reference-compare.json"
