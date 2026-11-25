#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail

root=$(cd -- "$(dirname -- "$0")/../.." && pwd)
work=${1:?Specify a writable build directory}
mkdir -p "$work"
work=$(cd -- "$work" && pwd)
output=${2:-"$root/firmware/tp_pcd3349a_behavioural.rom"}
upstream=https://github.com/Macroassembler-AS/asl-releases.git

git -C "$work" init -q
git -C "$work" fetch --depth 1 "$upstream" c7155b4fd3d33110f0eb098dede4295a8c008772
git -C "$work" checkout --detach FETCH_HEAD
git -C "$work" fetch --depth 1 "$upstream" 8c9f87fc8e478ba256235453a03a52de92af8915
git -C "$work" show FETCH_HEAD:Makefile.def > "$work/Makefile.def"
make -C "$work" -j2 asl p2bin

mkdir -p "$work/model"
cp "$root/tools/tp8048/rom/tp_pcd3349a_behavioural.asm" "$root/tools/tp8048/rom/tp_pcd3349a_symbols.inc" "$work/model/"
PATH="$work:$PATH" python3 "$root/tools/tp8048/build_tp_rom.py" \
  --src "$work/model/tp_pcd3349a_behavioural.asm" --out "$output"
