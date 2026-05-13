#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Wrapper: maps --firmware / --board style arguments to COINLINE_* env vars, then execs mame.
#
# Usage:
#   ./scripts/run-millennium.sh --firmware ../firmware/flash.bin millennium [extra mame args...]

set -euo pipefail

firmware=""
board=""
extra=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    --firmware)
      firmware="${2:-}"
      shift 2
      ;;
    --board)
      board="${2:-}"
      shift 2
      ;;
    *)
      extra+=("$1")
      shift
      ;;
  esac
done

here="$(cd "$(dirname "$0")" && pwd)"
emu_root="$(cd "$here/.." && pwd)"
export COINLINE_EMU_ROOT="$emu_root"
if [[ -n "$firmware" ]]; then
  export COINLINE_FIRMWARE="$firmware"
fi
if [[ -n "$board" ]]; then
  export COINLINE_BOARD="$board"
fi

exec mame "${extra[@]}"
