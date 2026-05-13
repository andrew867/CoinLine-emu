#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copy the CoinLine millennium driver into an external MAME tree and print the
# src/mame/mame.lst lines to register (or merge into a coinline subtarget list).
#
# Environment:
#   EXTERNAL_MAME_ROOT  Absolute path to the MAME repository root (required).
#   COINLINE_EMU_ROOT   Absolute path to this coinline-emu directory (default: parent of this script).
#
# Usage: ./scripts/register-millennium-driver.sh [--dry-run]

set -euo pipefail

dry_run=0
if [[ "${1:-}" == "--dry-run" ]]; then
  dry_run=1
fi

if [[ -z "${EXTERNAL_MAME_ROOT:-}" ]]; then
  echo "register-millennium-driver.sh: set EXTERNAL_MAME_ROOT to your MAME checkout." >&2
  exit 1
fi

here="$(cd "$(dirname "$0")" && pwd)"
emu_root="${COINLINE_EMU_ROOT:-$(cd "$here/.." && pwd)}"
dest_dir="$EXTERNAL_MAME_ROOT/src/mame/coinline"
src_dir="$emu_root/src/mame/coinline"
mame_lst="$EXTERNAL_MAME_ROOT/src/mame/mame.lst"

lst_files=(
  millennium.cpp
  millennium_audio_route_apply.cpp
  millennium_state.cpp
  millennium_memory.cpp
  millennium_microwire_93c66.cpp
  millennium_io.cpp
  millennium_debug.cpp
  millennium_board_hwinit.cpp
  millennium_firmware.cpp
  millennium_io_shared.cpp
  millennium_voiceware_config.cpp
  millennium_voiceware_phrase_lookup.cpp
  millennium_voiceware_phrase_port.cpp
  millennium_sha256.cpp
  millennium_vfd.cpp
  millennium_vfd_model.cpp
  millennium_keypad.cpp
  millennium_keypad_model.cpp
  millennium_security.cpp
  millennium_security_model.cpp
  millennium_modem.cpp
  millennium_modem_model.cpp
  millennium_hostbridge.cpp
  millennium_hostbridge_tcp.cpp
  millennium_nvram.cpp
  millennium_nvram_model.cpp
  millennium_card.cpp
  millennium_card_model.cpp
  millennium_smartcard.cpp
  millennium_smartcard_model.cpp
  millennium_sam.cpp
  millennium_coin.cpp
  millennium_coin_model.cpp
  millennium_audio.cpp
  millennium_audio_model.cpp
  millennium_audio_route.cpp
  millennium_audio_trace.cpp
  millennium_voiceware.cpp
  millennium_telephony.cpp
  millennium_evidence_bundle.cpp
)

echo "CoinLine emu root: $emu_root"
echo "MAME root:         $EXTERNAL_MAME_ROOT"
echo "Driver source:     $src_dir"
echo "Driver dest:       $dest_dir"

if [[ "$dry_run" -eq 0 ]]; then
  mkdir -p "$dest_dir"
  shopt -s nullglob
  for f in "$src_dir"/*.cpp "$src_dir"/*.h; do
    base="$(basename "$f")"
    cp "$f" "$dest_dir/$base"
  done
  shopt -u nullglob
fi

echo ""
echo "Ensure these lines exist in your MAME driver list (example file: $mame_lst):"
for f in "${lst_files[@]}"; do
  echo "src/mame/coinline/$f"
done

echo ""
echo "Then from MAME root, rebuild (example):"
echo "  make REGENIE=1 -j\"\$(nproc)\""
echo ""
echo "Runtime environment (set before launching mame):"
echo "  COINLINE_FIRMWARE=../firmware/flash.bin"
echo "  COINLINE_BOARD=fixtures/board/board-profile-2line-vfd.json"
echo "  COINLINE_NEW_HARDWARE_REVISION_1=0   # optional: legacy PIO port A image (else 0xBF)"
echo "  COINLINE_EMU_ROOT=$emu_root"
echo "  COINLINE_BOOT_TRACE=boot-trace.jsonl"
echo "  COINLINE_UNKNOWN_PORT_LOG=unknown-port.jsonl"
