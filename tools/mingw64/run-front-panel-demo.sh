#!/usr/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Visible front-panel capture using OS/MAME input delivery through the MAME input system.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EMU_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$EMU_ROOT"

export COINLINE_RUN_KIND="${COINLINE_RUN_KIND:-front-panel-demo}"
export COINLINE_TRACE_PROFILE="${COINLINE_TRACE_PROFILE:-uart}"
export COINLINE_BOOT_CAPTURE_SECONDS="${COINLINE_BOOT_CAPTURE_SECONDS:-45}"
export COINLINE_REAL_INPUT_DEMO=1
export COINLINE_SCRIPTED_PANEL_DEMO="${COINLINE_SCRIPTED_PANEL_DEMO:-0}"
export COINLINE_BOOT_CAPTURE_HEADLESS=0
export COINLINE_BOOT_CAPTURE_AUDIO="${COINLINE_BOOT_CAPTURE_AUDIO:-1}"
# Alias optional COINLINE_WAVWRITE=1 to WAV capture flag consumed by run-boot-critical-capture.sh
if [[ "${COINLINE_WAVWRITE:-0}" == "1" ]]; then
	export COINLINE_BOOT_CAPTURE_WAV_WRITE=1
fi

"$SCRIPT_DIR/run-boot-critical-capture.sh" -RealInputDemo
