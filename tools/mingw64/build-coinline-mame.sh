#!/usr/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Overlay CoinLine into MAME, build SUBTARGET=coinline, copy coinline-mame.exe, write build-result.json.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EMU_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
export MSYSTEM="${MSYSTEM:-MINGW64}"
export CHERE_INVOKING=1
export PATH="/mingw64/bin:/usr/bin:/bin:${PATH}"

cd "$EMU_ROOT"

MAME_ROOT="${EXTERNAL_MAME_ROOT:-${COINLINE_MAME_ROOT:-$EMU_ROOT/third-party/mame}}"
export EXTERNAL_MAME_ROOT="$MAME_ROOT"

JOBS="${COINLINE_MAKE_JOBS:-}"
if [[ -z "$JOBS" || ! "$JOBS" =~ ^[0-9]+$ ]]; then
	JOBS="${NUMBER_OF_PROCESSORS:-4}"
fi
if (( JOBS > 6 )); then JOBS=6; fi
export COINLINE_MAKE_JOBS="$JOBS"
export COINLINE_EMU_ROOT="$EMU_ROOT"
export COINLINE_MSYS_ROOT="${COINLINE_MSYS_ROOT:-/c/msys64}"

LOG="$EMU_ROOT/build/logs/mame-build.log"
mkdir -p "$(dirname "$LOG")" "$EMU_ROOT/build"
if ! : >"$LOG" 2>/dev/null; then
	LOG="$EMU_ROOT/build/logs/mame-build-$(date -u +%Y%m%dT%H%M%SZ).log"
	: >"$LOG"
fi

if ! command -v cygpath >/dev/null 2>&1; then
	echo "ERROR: cygpath not found — run this script from MSYS2, not plain Git Bash without MSYS root." | tee -a "$LOG"
	exit 1
fi
EMU_WIN="$(cygpath -aw "$EMU_ROOT")"
MAME_WIN="$(cygpath -aw "$MAME_ROOT")"

echo "build-coinline-mame.sh: EMU_ROOT=$EMU_ROOT MAME_ROOT=$MAME_ROOT jobs=$JOBS" | tee -a "$LOG"

PS_EXE="$(command -v powershell.exe 2>/dev/null || true)"
if [[ -z "$PS_EXE" && -x /c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe ]]; then
	PS_EXE="/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe"
fi
if [[ -z "$PS_EXE" ]]; then
	echo "ERROR: powershell.exe not found (needed for overlay step). Add it to PATH or install Windows PowerShell." | tee -a "$LOG"
	exit 1
fi

"$PS_EXE" -NoProfile -ExecutionPolicy Bypass -File "$EMU_ROOT/tools/windows/overlay-coinline-driver.ps1" -EmuRoot "$EMU_WIN" -MameRoot "$MAME_WIN" 2>&1 | tee -a "$LOG"

INNER="$EMU_ROOT/tools/windows/_build_mame_inner.sh"
export SHELL=/usr/bin/bash
echo "--- make (see _build_mame_inner.sh) ---" | tee -a "$LOG"
bash -euo pipefail "$INNER"

BIN_DIR="$EMU_ROOT/build/bin"
mkdir -p "$BIN_DIR"
DEST="$BIN_DIR/coinline-mame.exe"

EXE=""
for c in "$MAME_ROOT/mamecoinline.exe" "$MAME_ROOT/mame.exe" "$MAME_ROOT/mame64.exe"; do
	if [[ -f "$c" ]]; then EXE="$c"; break; fi
done

TS="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
BR_JSON="$EMU_ROOT/build/build-result.json"
export COINLINE_BR_JSON_WIN="$(cygpath -w "$BR_JSON")"
export COINLINE_DEST_WIN="$(cygpath -w "$DEST")"
export COINLINE_LOG_WIN="$(cygpath -w "$LOG")"
export COINLINE_MAME_WIN="$(cygpath -w "$MAME_ROOT")"
export COINLINE_TS="$TS"

if [[ -z "$EXE" ]]; then
	python3 <<'PY'
import json, os
outp = os.environ["COINLINE_BR_JSON_WIN"]
os.makedirs(os.path.dirname(outp), exist_ok=True)
p = os.environ.get("COINLINE_DEST_WIN", "")
doc = {
    "command": "build-coinline-mame.sh -> make REGENIE=1 SUBTARGET=coinline NOWERROR=1",
    "exit_code": 1,
    "executable_path": p,
    "executable_exists": False,
    "log_path": os.environ.get("COINLINE_LOG_WIN", ""),
    "success": False,
    "mame_root": os.environ.get("COINLINE_MAME_WIN", ""),
    "timestamp_utc": os.environ.get("COINLINE_TS", ""),
}
with open(outp, "w", encoding="utf-8") as f:
    f.write(json.dumps(doc, indent=2))
print(json.dumps(doc, indent=2))
PY
	echo "ERROR: mamecoinline.exe not found under $MAME_ROOT" | tee -a "$LOG"
	exit 1
fi

cp -f "$EXE" "$DEST"
HASH="$(sha256sum "$DEST" | awk '{print $1}')"
export COINLINE_EXE_WIN="$(cygpath -w "$EXE")"
export COINLINE_HASH="$HASH"
python3 <<'PY'
import json, os
outp = os.environ["COINLINE_BR_JSON_WIN"]
os.makedirs(os.path.dirname(outp), exist_ok=True)
doc = {
    "command": "build-coinline-mame.sh -> make REGENIE=1 SUBTARGET=coinline NOWERROR=1",
    "exit_code": 0,
    "elapsed_ms": None,
    "executable_path": os.environ["COINLINE_DEST_WIN"],
    "source_executable": os.environ["COINLINE_EXE_WIN"],
    "executable_exists": True,
    "executable_sha256": os.environ["COINLINE_HASH"],
    "log_path": os.environ["COINLINE_LOG_WIN"],
    "success": True,
    "mame_root": os.environ["COINLINE_MAME_WIN"],
    "timestamp_utc": os.environ["COINLINE_TS"],
}
with open(outp, "w", encoding="utf-8") as f:
    f.write(json.dumps(doc, indent=2))
print(json.dumps(doc, indent=2))
PY

echo "build-coinline-mame.sh: OK -> $DEST" | tee -a "$LOG"
