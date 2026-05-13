#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Build helper for the MAME engine track. Invokes an external MAME source tree;
# the CoinLine machine driver is added in later tranches (E2+).
#
# Environment:
#   EXTERNAL_MAME_ROOT  Absolute path to the MAME repository root (required to build).
#   MAME_MAKE_JOBS    Optional parallel job count (default: nproc or 4).
#
# Usage: ./scripts/build-mame.sh [--help]

set -euo pipefail

usage() {
  sed -n '1,22p' "$0" | tail -n +2
}

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
  usage
  exit 0
fi

if [[ -z "${EXTERNAL_MAME_ROOT:-}" ]]; then
  echo "build-mame.sh: EXTERNAL_MAME_ROOT is not set." >&2
  echo "  Export it to your MAME checkout, then re-run. Example:" >&2
  echo "    export EXTERNAL_MAME_ROOT=/path/to/mame" >&2
  echo "  (No build performed.)" >&2
  exit 0
fi

if [[ ! -d "$EXTERNAL_MAME_ROOT" ]]; then
  echo "build-mame.sh: EXTERNAL_MAME_ROOT is not a directory: $EXTERNAL_MAME_ROOT" >&2
  exit 1
fi

jobs="${MAME_MAKE_JOBS:-}"
if [[ -z "$jobs" ]]; then
  if command -v nproc >/dev/null 2>&1; then
    jobs="$(nproc)"
  else
    jobs=4
  fi
fi

echo "build-mame.sh: using MAME tree: $EXTERNAL_MAME_ROOT"
echo "build-mame.sh: NOTE — after registering the millennium driver (scripts/register-millennium-driver.sh),"
echo "  add its .cpp lines to src/mame/mame.lst (or a coinline subtarget list) and rebuild."

cd "$EXTERNAL_MAME_ROOT"
# Example when a dedicated subtarget exists: make SUBTARGET=coinline ...
make -j"$jobs"
