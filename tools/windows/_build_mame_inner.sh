#!/usr/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Invoked from MSYS2 with COINLINE_EMU_ROOT and EXTERNAL_MAME_ROOT set.
set -euo pipefail
: "${COINLINE_EMU_ROOT:?}"
: "${EXTERNAL_MAME_ROOT:?}"
: "${COINLINE_MAKE_JOBS:?}"

# Non-login child shells may not inherit mingw64/ucrt64 bin first; ensure gcc is found for GCC_VERSION.
case "${MSYSTEM:-}" in
MINGW64)
	export MINGW_PREFIX="${MINGW_PREFIX:-/mingw64}"
	export MINGW64="${MINGW64:-/mingw64}"
	export PTR64="${PTR64:-1}"
	export PATH="/mingw64/bin:/usr/bin:/bin:$PATH"
	;;
UCRT64)
	export MINGW_PREFIX="${MINGW_PREFIX:-/ucrt64}"
	export MINGW64="${MINGW64:-/ucrt64}"
	export PTR64="${PTR64:-1}"
	export PATH="/ucrt64/bin:/usr/bin:/bin:$PATH"
	;;
CLANG64)
	export MINGW_PREFIX="${MINGW_PREFIX:-/clang64}"
	export MINGW64="${MINGW64:-/clang64}"
	export PTR64="${PTR64:-1}"
	export PATH="/clang64/bin:/usr/bin:/bin:$PATH"
	;;
MSYS) export PATH="/usr/bin:/bin:$PATH" ;;
*)
	export MINGW_PREFIX="${MINGW_PREFIX:-/mingw64}"
	export MINGW64="${MINGW64:-/mingw64}"
	export PTR64="${PTR64:-1}"
	export PATH="/mingw64/bin:/usr/bin:/bin:$PATH"
	;;
esac

# GNU Make on Windows defaults to cmd.exe unless SHELL contains /bin; MAME needs posix shell + GCC probes.
export SHELL=/usr/bin/bash
# MAME makefile uses $(OS) to pick the Windows vs uname() branch; MSYS2 bash often has OS unset.
export OS="${OS:-Windows_NT}"

export EXTERNAL_MAME_ROOT
export COINLINE_EMU_ROOT

log="${COINLINE_EMU_ROOT}/build/logs/mame-build.log"
mkdir -p "$(dirname "$log")"
exec > >(tee -a "$log") 2>&1

echo "_build_mame_inner.sh: MAME root=${EXTERNAL_MAME_ROOT} jobs=${COINLINE_MAKE_JOBS}"

cd "${EXTERNAL_MAME_ROOT}"

if [[ ! -f scripts/target/mame/coinline.lua ]]; then
	echo "ERROR: scripts/target/mame/coinline.lua missing — run overlay-coinline-driver.ps1 first." >&2
	exit 1
fi
if [[ ! -f src/mame/coinline.lst ]]; then
	echo "ERROR: src/mame/coinline.lst missing — run overlay-coinline-driver.ps1 first." >&2
	exit 1
fi

j="${COINLINE_MAKE_JOBS}"
if [[ -z "$j" || ! "$j" =~ ^[0-9]+$ ]]; then j=4; fi
if (( j > 6 )); then j=6; fi
echo "make REGENIE=1 SUBTARGET=coinline NOWERROR=1 -j${j}"
msys="${COINLINE_MSYS_ROOT:-}"
if [[ -n "$msys" && -x "$msys/usr/bin/make" ]]; then
	MAKEBIN="$msys/usr/bin/make"
else
	MAKEBIN="$(command -v make)"
fi
# NOWERROR=1: GCC 14+ may warn on SFINAE/incomplete types in emu headers; without this -Werror aborts precompile.
"$MAKEBIN" SHELL=/usr/bin/bash REGENIE=1 SUBTARGET=coinline NOWERROR=1 -j"${j}"
