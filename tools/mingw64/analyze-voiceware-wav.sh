#!/usr/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EMU_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$EMU_ROOT"

export MSYSTEM="${MSYSTEM:-MINGW64}"
export CHERE_INVOKING=1
export PATH="/mingw64/bin:/usr/bin:/bin:${PATH}"

if [[ $# -lt 1 ]]; then
	echo "usage: analyze-voiceware-wav.sh <wav>" >&2
	exit 2
fi

mkdir -p "$EMU_ROOT/build/tools"
EXE="$EMU_ROOT/build/tools/voiceware_decoder_sweep.exe"
if [[ ! -x "$EXE" || "$EMU_ROOT/tools/voiceware/voiceware_decoder_sweep.cpp" -nt "$EXE" ]]; then
	g++ -std=c++17 -O2 -Wall -Wextra -pedantic "$EMU_ROOT/tools/voiceware/voiceware_decoder_sweep.cpp" -o "$EXE"
fi
"$EXE" --analyze-wav "$1"
