#!/usr/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EMU_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$EMU_ROOT"

export MSYSTEM="${MSYSTEM:-MINGW64}"
export CHERE_INVOKING=1
export PATH="/mingw64/bin:/usr/bin:/bin:${PATH}"

PHRASE="${COINLINE_VOICEWARE_SWEEP_PHRASE:-0x3f}"
BANK="${COINLINE_VOICEWARE_SWEEP_BANK:-15}"
TS="$(date +%Y%m%dT%H%M%S)"
OUT="${COINLINE_VOICEWARE_SWEEP_OUT:-$EMU_ROOT/build/runs/${TS}-voiceware-decoder-sweep}"
mkdir -p "$OUT" "$EMU_ROOT/build/tools"

LATEST="${EMU_ROOT}/build/runs/latest-boot-critical.txt"
if [[ -z "${COINLINE_VOICE_ROM_U16:-}" || -z "${COINLINE_VOICE_ROM_U26:-}" ]]; then
	if [[ -f "$LATEST" ]]; then
		LATEST_RUN="$(tr -d '\r\n' < "$LATEST")"
		DEFAULT_U16="${LATEST_RUN}/mame-rompath/cl_millennium/voice_a.bin"
		DEFAULT_U26="${LATEST_RUN}/mame-rompath/cl_millennium/voice_b.bin"
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

EXE="$EMU_ROOT/build/tools/voiceware_decoder_sweep.exe"
g++ -std=c++17 -O2 -Wall -Wextra -pedantic "$EMU_ROOT/tools/voiceware/voiceware_decoder_sweep.cpp" -o "$EXE"
"$EXE" "$U16" "$U26" "$OUT" "$PHRASE" "$BANK"
echo "$OUT" > "$EMU_ROOT/build/runs/latest-voiceware-decoder-sweep.txt"
echo "OK: voiceware decoder sweep $OUT"

