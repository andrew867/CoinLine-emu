# SPDX-License-Identifier: GPL-2.0-or-later
#requires -Version 5.1
param(
    [string] $EmuRoot = '',
    [string] $MameRoot = ''
)

$ErrorActionPreference = 'Stop'
if (-not $EmuRoot) {
    $EmuRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
}
if (-not $MameRoot) {
    if ($env:EXTERNAL_MAME_ROOT) {
        $MameRoot = $env:EXTERNAL_MAME_ROOT
    }
    else {
        $MameRoot = Join-Path $EmuRoot 'third-party\mame'
    }
}
if (-not (Test-Path -LiteralPath $MameRoot)) {
    throw "MAME root not found: $MameRoot"
}

$src = Join-Path $EmuRoot 'src\mame\coinline'
if (-not (Test-Path -LiteralPath $src)) {
    throw "Driver source missing: $src"
}
$dest = Join-Path $MameRoot 'src\mame\coinline'
New-Item -ItemType Directory -Force -Path $dest | Out-Null

$manifest = [ordered]@{ copied = @(); timestamp_utc = (Get-Date).ToUniversalTime().ToString('o') }
Get-ChildItem -LiteralPath $src -File | ForEach-Object {
    $target = Join-Path $dest $_.Name
    Copy-Item -LiteralPath $_.FullName -Destination $target -Force
    $manifest.copied += $_.Name
}

$overlayLua = Join-Path $EmuRoot 'mame-overlays\scripts\target\mame\coinline.lua'
$destLua = Join-Path $MameRoot 'scripts\target\mame\coinline.lua'
if (-not (Test-Path -LiteralPath $overlayLua)) {
    throw "Missing overlay lua: $overlayLua"
}
New-Item -ItemType Directory -Force -Path (Split-Path $destLua) | Out-Null
Copy-Item -LiteralPath $overlayLua -Destination $destLua -Force
$manifest.copied += 'scripts/target/mame/coinline.lua'

$build = Join-Path $EmuRoot 'build'
New-Item -ItemType Directory -Force -Path $build | Out-Null
$extra = @(
    @{ rel = 'mame-overlays\src\mame\coinline.cpp'; destRel = 'src\mame\coinline.cpp' },
    @{ rel = 'mame-overlays\src\mame\coinline.lst'; destRel = 'src\mame\coinline.lst' }
)
foreach ($e in $extra) {
    $srcF = Join-Path $EmuRoot $e.rel
    if (-not (Test-Path -LiteralPath $srcF)) {
        throw "Missing overlay file: $srcF"
    }
    $dstF = Join-Path $MameRoot $e.destRel
    New-Item -ItemType Directory -Force -Path (Split-Path $dstF) | Out-Null
    Copy-Item -LiteralPath $srcF -Destination $dstF -Force
    $manifest.copied += $e.destRel
}

$layoutSrc = Join-Path $EmuRoot 'src\mame\layout\millennium.lay'
if (Test-Path -LiteralPath $layoutSrc) {
    $layoutDstDir = Join-Path $MameRoot 'src\mame\layout'
    New-Item -ItemType Directory -Force -Path $layoutDstDir | Out-Null
    Copy-Item -LiteralPath $layoutSrc -Destination (Join-Path $layoutDstDir 'millennium.lay') -Force
    $manifest.copied += 'src/mame/layout/millennium.lay'
}

$manifest | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $build 'overlay-manifest.json') -Encoding UTF8
Write-Host "Overlay complete -> $dest + coinline.cpp/lst ($($manifest.copied.Count) entries)"
exit 0
