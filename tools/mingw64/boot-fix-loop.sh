#!/usr/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Up to 5 cycles: build -> 180s capture -> validate -> optional analysis stub
set -euo pipefail
MAX="${1:-5}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EMU_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
export MSYSTEM="${MSYSTEM:-MINGW64}"
export CHERE_INVOKING=1
export PATH="/mingw64/bin:/usr/bin:/bin:${PATH}"
cd "$EMU_ROOT"
LOG="${EMU_ROOT}/build/logs/boot-fix-loop.log"
mkdir -p "$(dirname "$LOG")"
: >"$LOG"

for ((i = 1; i <= MAX; i++)); do
	{
		echo "======== cycle $i / $MAX ========"
		echo "+ ./tools/mingw64/build-coinline-mame.sh"
	} | tee -a "$LOG"
	./tools/mingw64/build-coinline-mame.sh 2>&1 | tee -a "$LOG" || {
		echo "build failed" | tee -a "$LOG"
		exit 1
	}
	{
		echo "+ ./tools/mingw64/run-boot-critical-capture.sh"
	} | tee -a "$LOG"
	./tools/mingw64/run-boot-critical-capture.sh 2>&1 | tee -a "$LOG" || true
	RUN_DIR="$(tr -d '\r\n' < "${EMU_ROOT}/build/runs/latest-boot-critical.txt" 2>/dev/null || true)"
	if [[ -n "$RUN_DIR" && -d "$RUN_DIR" ]]; then
		SUM="${RUN_DIR}/boot-fix-loop-summary.md"
		{
			echo "# boot-fix-loop cycle $i"
			echo "- run: \`$RUN_DIR\`"
			echo "- time: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
		} >"$SUM"
		# lightweight analysis: line count
		for f in io-trace.jsonl boot-trace.jsonl cpu-trace.jsonl; do
			[[ -f "$RUN_DIR/$f" ]] && echo "- $f lines: $(wc -l <"$RUN_DIR/$f" | tr -d ' ')" >>"$SUM"
		done
	fi
	if ./tools/mingw64/validate-latest-boot.sh 2>&1 | tee -a "$LOG"; then
		echo "$RUN_DIR" >"${EMU_ROOT}/build/runs/latest-m6-success.txt"
		{
			echo "SUCCESS: M6 evidence in $RUN_DIR"
		} | tee -a "$LOG"
		exit 0
	fi
	echo "cycle $i: M6 not proven; see run folder and boot-blocker.md" | tee -a "$LOG"
done
echo "exhausted $MAX cycles without M6" | tee -a "$LOG"
exit 1
