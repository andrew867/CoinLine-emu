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
OUT="${COINLINE_VOICEWARE_VALIDATE_OUT:-$EMU_ROOT/build/runs/${TS}-voiceware-reference-validate}"
RUN_DIR="${COINLINE_VOICEWARE_RUN_DIR:-}"
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
	echo "ERROR: voice ROMs not found. Set COINLINE_VOICE_ROM_U16 and COINLINE_VOICE_ROM_U26 or pass paths as args." >&2
	exit 2
fi

REF_C="$EMU_ROOT/build/tools/nortel-voiceware-decoder.c"
REF_EXE="$EMU_ROOT/build/tools/nortel-voiceware-decoder.exe"
REF_URL="https://raw.githubusercontent.com/hharte/nortel-voiceware-decoder/refs/heads/main/nortel-voiceware-decoder.c"

curl -fsSL "$REF_URL" -o "$REF_C"
gcc -O2 -Wall -Wextra -std=c11 "$REF_C" -o "$REF_EXE"

run_decode() {
	local rom_path="$1"
	local rom_tag="$2"
	local decode_dir="$OUT/$rom_tag"
	local stdout_file="$OUT/${rom_tag}.stdout.log"
	local stderr_file="$OUT/${rom_tag}.stderr.log"
	mkdir -p "$decode_dir"
	pushd "$decode_dir" >/dev/null
	if "$REF_EXE" "$rom_path" >"$stdout_file" 2>"$stderr_file"; then
		echo "ok"
	else
		echo "failed"
	fi
	popd >/dev/null
}

U16_STATUS="$(run_decode "$U16" "u16")"
U26_STATUS="$(run_decode "$U26" "u26")"

export OUT U16 U26 U16_STATUS U26_STATUS
python - <<'PY'
import json, pathlib, os

out = pathlib.Path(os.environ["OUT"])

def summarize(tag, rom_path, status):
    stderr_path = out / f"{tag}.stderr.log"
    stdout_path = out / f"{tag}.stdout.log"
    stderr = stderr_path.read_text(encoding="utf-8", errors="ignore") if stderr_path.exists() else ""
    stdout = stdout_path.read_text(encoding="utf-8", errors="ignore") if stdout_path.exists() else ""
    wav_count = len(list((out / tag).glob("*.wav")))
    pcm_count = len(list((out / tag).glob("*.pcm")))
    return {
        "rom_file": str(pathlib.Path(rom_path)),
        "decode_status": status,
        "wav_files_generated": wav_count,
        "pcm_files_generated": pcm_count,
        "stderr_has_error": ("ERROR:" in stderr),
        "stderr_tail": "\n".join(stderr.splitlines()[-20:]),
        "stdout_tail": "\n".join(stdout.splitlines()[-20:]),
        "valid_decode": status == "ok" and "ERROR:" not in stderr and (wav_count + pcm_count) > 0
    }

report = {
    "schema_version": "coinline.voiceware_reference_validate/v1",
    "reference_decoder_source": "hharte/nortel-voiceware-decoder.c",
    "u16": summarize("u16", os.environ["U16"], os.environ["U16_STATUS"]),
    "u26": summarize("u26", os.environ["U26"], os.environ["U26_STATUS"]),
}
report["all_roms_valid"] = bool(report["u16"]["valid_decode"] and report["u26"]["valid_decode"])

(out / "voiceware-reference-validate-summary.json").write_text(
    json.dumps(report, indent=2) + "\n", encoding="utf-8"
)
PY

echo "$OUT" > "$EMU_ROOT/build/runs/latest-voiceware-reference-validate.txt"
echo "OK: voiceware reference validate $OUT"
