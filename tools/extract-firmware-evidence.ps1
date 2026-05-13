# SPDX-License-Identifier: GPL-2.0-or-later
#requires -Version 5.1
<#
.SYNOPSIS
  Scan FIRMWARE_SOURCE_ROOT for linker maps, vectors, and hardware headers; emit JSON + internal doc stubs.

.DESCRIPTION
  Fails loudly if FIRMWARE_SOURCE_ROOT is missing or no evidence files are found (no mock maps).
#>
param(
    [string] $FirmwareSourceRoot = '',
    [string] $OutDir = ''
)

$ErrorActionPreference = 'Stop'

if (-not $FirmwareSourceRoot) {
    if (-not $env:FIRMWARE_SOURCE_ROOT) {
        throw 'Set -FirmwareSourceRoot or FIRMWARE_SOURCE_ROOT to the firmware source tree (e.g. firmware source tree checkout).'
    }
    $FirmwareSourceRoot = $env:FIRMWARE_SOURCE_ROOT
}
if (-not (Test-Path -LiteralPath $FirmwareSourceRoot)) {
    throw "FIRMWARE_SOURCE_ROOT not found: $FirmwareSourceRoot"
}

$EmuRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not $OutDir) {
    $OutDir = Join-Path $EmuRoot 'build\generated'
}
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$mapFiles = Get-ChildItem -LiteralPath $FirmwareSourceRoot -Recurse -File -Include *.map, *.xmap, *.nm 2>$null
$ldFiles = Get-ChildItem -LiteralPath $FirmwareSourceRoot -Recurse -File -Include *.ld, *.lds 2>$null
$hdrHits = Get-ChildItem -LiteralPath $FirmwareSourceRoot -Recurse -File -Include *.h, *.H, *.inc 2>$null |
    Where-Object { $_.Name -match 'IODEF|HW_|VFD|UART|NVRAM|MEM' } |
    Select-Object -First 80

if (-not $mapFiles -and -not $ldFiles -and -not $hdrHits) {
    throw "No firmware evidence found under $FirmwareSourceRoot (no .map/.ld, no matching headers). Refuse to emit placeholder JSON."
}

$inv = Join-Path $EmuRoot 'docs\internal\firmware-evidence-inventory.md'
$mapRel = $mapFiles | ForEach-Object { $_.FullName.Substring($FirmwareSourceRoot.Length).TrimStart('\', '/') }
$ldRel = $ldFiles | ForEach-Object { $_.FullName.Substring($FirmwareSourceRoot.Length).TrimStart('\', '/') }
$hdrRel = $hdrHits | ForEach-Object { $_.FullName.Substring($FirmwareSourceRoot.Length).TrimStart('\', '/') }

@"
# Firmware evidence inventory (generated)

- **Source root**: ``$FirmwareSourceRoot``
- **Generated UTC**: $((Get-Date).ToUniversalTime().ToString('o'))

## Map / linker files

$(
    if ($mapRel) { ($mapRel | ForEach-Object { "- ``$_``" }) -join "`n" } else { '(none found)' }
)

## Linker scripts

$(
    if ($ldRel) { ($ldRel | ForEach-Object { "- ``$_``" }) -join "`n" } else { '(none found)' }
)

## Header candidates (truncated)

$(
    if ($hdrRel) { ($hdrRel | ForEach-Object { "- ``$_``" }) -join "`n" } else { '(none)' }
)

> Next: curate constants into ``build/generated/*.json`` by hand or a follow-up extractor; this script only proves the tree contains linkable evidence.
"@ | Set-Content -LiteralPath $inv -Encoding UTF8

$mem = [ordered]@{
    schema_version = '0.1'
    source_root    = $FirmwareSourceRoot
    map_files       = @($mapRel)
    ld_files        = @($ldRel)
    note            = 'Populate regions from map + firmware source; do not invent addresses.'
}
$mem | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $OutDir 'memory-map.json') -Encoding UTF8

$io = [ordered]@{
    schema_version = '0.1'
    source_root = $FirmwareSourceRoot
    header_candidates = @($hdrRel)
    note = 'Merge with fixtures/board/io-port-map.json after human review.'
}
$io | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $OutDir 'io-port-map.json') -Encoding UTF8

$irq = [ordered]@{ schema_version = '0.1'; source_root = $FirmwareSourceRoot; note = 'Populate from interrupt controller / board headers.' }
$irq | ConvertTo-Json | Set-Content (Join-Path $OutDir 'interrupt-map.json') -Encoding UTF8

$sym = [ordered]@{ schema_version = '0.1'; source_root = $FirmwareSourceRoot; note = 'Populate from map symbols for RTOS/scheduler evidence.' }
$sym | ConvertTo-Json | Set-Content (Join-Path $OutDir 'boot-symbols.json') -Encoding UTF8

Write-Host "Evidence inventory written. JSON under $OutDir"
Write-Host "Updated $inv"

