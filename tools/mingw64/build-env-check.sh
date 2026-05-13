#!/usr/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Print MINGW64 toolchain facts (run under MSYS2 with MSYSTEM=MINGW64).
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EMU_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$EMU_ROOT"

LOG="${EMU_ROOT}/build/logs/mingw64-env-check.log"
mkdir -p "$(dirname "$LOG")"

run_check() {
	echo "=== mingw64-env-check $(date -u +%Y-%m-%dT%H:%M:%SZ) ==="
	echo "pwd=$(pwd)"
	echo "uname -a: $(uname -a)"
	echo "MSYSTEM=${MSYSTEM:-}"
	echo "PATH=${PATH}"
	echo "which cmake: $(command -v cmake 2>/dev/null || echo MISSING)"
	cmake --version 2>/dev/null || true
	echo "which ninja: $(command -v ninja 2>/dev/null || echo MISSING)"
	echo "which make: $(command -v make 2>/dev/null || echo MISSING)"
	echo "which gcc: $(command -v gcc 2>/dev/null || echo MISSING)"
	gcc --version 2>/dev/null || true
	echo "which python: $(command -v python 2>/dev/null || echo MISSING)"
	python --version 2>/dev/null || true
	echo "git --version: $(git --version 2>/dev/null || echo MISSING)"
	echo "--- git status --short (repo root) ---"
	(cd "$(git rev-parse --show-toplevel 2>/dev/null)" && git status --short) 2>/dev/null || true
}

run_check | tee "$LOG"
