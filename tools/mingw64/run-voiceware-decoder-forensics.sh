#!/usr/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EMU_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$EMU_ROOT"

export MSYSTEM="${MSYSTEM:-MINGW64}"
export CHERE_INVOKING=1
export PATH="/mingw64/bin:/usr/bin:/bin:${PATH}"

PHRASE="${COINLINE_VOICEWARE_AB_PHRASE:-0x3f}"
BANK="${COINLINE_VOICEWARE_AB_BANK:-15}"
RUN_DIR="${COINLINE_VOICEWARE_RUN_DIR:-}"
TS="$(date +%Y%m%dT%H%M%S)"
OUT="${COINLINE_VOICEWARE_FORENSICS_OUT:-$EMU_ROOT/build/runs/${TS}-voiceware-ab}"
mkdir -p "$OUT" "$EMU_ROOT/build/tools"

if [[ -z "$RUN_DIR" ]]; then
	LATEST_RUN_HINT="$EMU_ROOT/build/runs/latest-nis-craft-install-acceptance.txt"
	if [[ -f "$LATEST_RUN_HINT" ]]; then
		RUN_DIR="$(tr -d '\r\n' < "$LATEST_RUN_HINT")"
	fi
fi

if [[ -z "${COINLINE_VOICE_ROM_U16:-}" || -z "${COINLINE_VOICE_ROM_U26:-}" ]]; then
	if [[ -n "$RUN_DIR" ]]; then
		DEFAULT_U16="${RUN_DIR}/mame-rompath/cl_millennium/voice_a.bin"
		DEFAULT_U26="${RUN_DIR}/mame-rompath/cl_millennium/voice_b.bin"
	else
		DEFAULT_U16=""
		DEFAULT_U26=""
	fi
else
	DEFAULT_U16="$COINLINE_VOICE_ROM_U16"
	DEFAULT_U26="$COINLINE_VOICE_ROM_U26"
fi

U16="${1:-$DEFAULT_U16}"
U26="${2:-$DEFAULT_U26}"
if [[ -z "$U16" || -z "$U26" || ! -f "$U16" || ! -f "$U26" ]]; then
	echo "ERROR: voice ROMs not found. Set COINLINE_VOICE_ROM_U16 and COINLINE_VOICE_ROM_U26 or pass them as args." >&2
	exit 2
fi

EXE="$EMU_ROOT/build/tools/voiceware_decoder_ab.exe"
g++ -std=c++17 -O2 -Wall -Wextra -pedantic "$EMU_ROOT/tools/voiceware/voiceware_decoder_ab.cpp" -o "$EXE"
"$EXE" "$U16" "$U26" "$OUT" "$PHRASE" "$BANK"

export OUT PHRASE BANK U16 U26
python - <<'PY'
import json, pathlib, wave, struct, math, os
root = pathlib.Path(os.environ["OUT"])
phrase = os.environ["PHRASE"]
bank = int(os.environ["BANK"], 0)
u16_path = pathlib.Path(os.environ["U16"])
u26_path = pathlib.Path(os.environ["U26"])
summary = json.loads((root / "voiceware-ab-summary.json").read_text(encoding="utf-8"))
winner = summary.get("selected_candidate","")
cand_path = root / f"candidate-{winner}.json"
cand = json.loads(cand_path.read_text(encoding="utf-8")) if cand_path.exists() else {}
trace_src = root / "voiceware-decode-trace.jsonl"
trace_dst = root / "voiceware-decode-trace.jsonl"
if trace_src != trace_dst:
    trace_dst.write_text(trace_src.read_text(encoding="utf-8"), encoding="utf-8")

wav_path = root / cand.get("wav","")
dur = peak = rms = dc = 0.0
nz0 = nz1 = 0
if wav_path.exists():
    with wave.open(str(wav_path), "rb") as w:
        n = w.getnframes()
        sr = w.getframerate()
        raw = w.readframes(n)
    vals = struct.unpack("<" + "h" * (len(raw)//2), raw) if raw else []
    dur = (len(vals)/sr) if sr else 0.0
    if vals:
        absvals = [abs(v) for v in vals]
        peak = max(absvals)
        rms = math.sqrt(sum(v*v for v in vals)/len(vals))
        dc = sum(vals)/len(vals)
        nz = [i for i,v in enumerate(vals) if v != 0]
        if nz:
            nz0, nz1 = nz[0], nz[-1]

block_lines = []
if trace_src.exists():
    for ln in trace_src.read_text(encoding="utf-8", errors="ignore").splitlines():
        try:
            obj = json.loads(ln)
        except Exception:
            continue
        if obj.get("event") == "block_start":
            op = obj.get("raw_control", obj.get("control", ""))
            op_class = obj.get("op_class", "")
            btype = obj.get("block_type", "")
            rate = obj.get("sample_rate", "")
            count = obj.get("sample_count", "")
            block_lines.append(
                f'{obj.get("offset","")} op={op} class={op_class} type={btype} rate={rate} count={count}'
            )
(root / "voiceware-block-map.txt").write_text("\n".join(block_lines) + ("\n" if block_lines else ""), encoding="utf-8")

rom_path = u16_path if bank < 8 else u26_path
rom_bytes = rom_path.read_bytes()
bank_window_size = 0x20000
bank_in_chip = bank & 0x07
window_base = bank_in_chip * bank_window_size
directory_offset = 5 + (int(phrase, 0) & 0xFF) * 2
dir_hi = rom_bytes[window_base + directory_offset] if window_base + directory_offset < len(rom_bytes) else 0
dir_lo = rom_bytes[window_base + directory_offset + 1] if window_base + directory_offset + 1 < len(rom_bytes) else 0
vector_be = ((dir_hi << 8) | dir_lo)
vector_le = ((dir_lo << 8) | dir_hi)
offset_be = vector_be * 2 + 1
offset_le = vector_le * 2 + 1
offset_be_ok = 0 <= offset_be < bank_window_size
offset_le_ok = 0 <= offset_le < bank_window_size
op_be = f"0x{rom_bytes[window_base + offset_be]:02X}" if offset_be_ok and (window_base + offset_be) < len(rom_bytes) else "out_of_range"
op_le = f"0x{rom_bytes[window_base + offset_le]:02X}" if offset_le_ok and (window_base + offset_le) < len(rom_bytes) else "out_of_range"
selected_endian = "big" if cand.get("decoded_start_offset", "").lower() == f"0x{offset_be:06x}" else ("little" if cand.get("decoded_start_offset", "").lower() == f"0x{offset_le:06x}" else "unknown")

address_forensics = {
    "schema_version": "coinline.voiceware_address_forensics/v1",
    "phrase_index": phrase,
    "bank_number": bank,
    "rom_file_name": rom_path.name,
    "rom_size_bytes": len(rom_bytes),
    "bank_window_size_bytes": bank_window_size,
    "physical_bank_base": f"0x{window_base:06X}",
    "directory_location_in_bank": f"0x{directory_offset:06X}",
    "directory_vector_bytes": [f"0x{dir_hi:02X}", f"0x{dir_lo:02X}"],
    "vector_candidates": {
        "big_endian": {
            "vector_word": f"0x{vector_be:04X}",
            "vector_times_2_plus_1": f"0x{offset_be:06X}",
            "offset_inside_128k_window": offset_be_ok,
            "first_opcode_candidate": op_be
        },
        "little_endian": {
            "vector_word": f"0x{vector_le:04X}",
            "vector_times_2_plus_1": f"0x{offset_le:06X}",
            "offset_inside_128k_window": offset_le_ok,
            "first_opcode_candidate": op_le
        }
    },
    "selected_vector_endian": selected_endian,
    "selected_decoded_offset": cand.get("decoded_start_offset", ""),
    "selected_parser_candidate": winner
}
(root / "voiceware-address-forensics.json").write_text(json.dumps(address_forensics, indent=2) + "\n", encoding="utf-8")

forensics = {
    "schema_version": "coinline.voiceware_phrase_forensics/v1",
    "phrase": phrase,
    "bank": bank,
    "rom": cand.get("ROM",""),
    "directory_offset": cand.get("directory_offset",""),
    "decoded_start_offset": cand.get("decoded_start_offset",""),
    "selected_candidate": winner,
    "variable_length_semantics": cand.get("variable_length_semantics",""),
    "nibble_order": cand.get("nibble_order",""),
    "predictor_index_mode": cand.get("predictor_index_mode",""),
    "sample_rate_markers_seen": cand.get("sample_rate_markers_seen",[]),
    "output_sample_rate": cand.get("output_sample_rate",0),
    "wav": cand.get("wav",""),
    "wav_duration_s": dur,
    "nonzero_region_samples": [nz0, nz1],
    "peak_abs_int16": peak,
    "rms": rms,
    "dc_offset": dc,
}
(root / "voiceware-phrase-forensics.json").write_text(json.dumps(forensics, indent=2) + "\n", encoding="utf-8")

ab_summary = {
    "schema_version": "coinline.voiceware_ab_summary/v1",
    "selected_candidate": winner,
    "selection_reason": summary.get("selection_reason",""),
    "candidates": summary.get("candidates",[])
}
(root / "voiceware-ab-summary.json").write_text(json.dumps(ab_summary, indent=2) + "\n", encoding="utf-8")
PY

echo "$OUT" > "$EMU_ROOT/build/runs/latest-voiceware-ab.txt"
echo "OK: voiceware decoder forensics $OUT"
