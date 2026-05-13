# SPDX-License-Identifier: GPL-2.0-or-later
#requires -Version 5.1
<#
.SYNOPSIS
  Concatenate the primary + secondary flash images into a single flash image
  for emulator experiments (does not modify sources).
#>
param(
    [string] $Flash0 = '..\firmware\flash.bin',
    [string] $Flash1 = '..\firmware\flash1.bin',
    [string] $OutputPath = ''
)

$ErrorActionPreference = 'Stop'
$EmuRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not $OutputPath) {
    $outDir = Join-Path $EmuRoot 'build\firmware'
    New-Item -ItemType Directory -Force -Path $outDir | Out-Null
    $OutputPath = Join-Path $outDir 'flash-image.bin'
}
if (-not (Test-Path -LiteralPath $Flash0)) { throw "Primary flash image not found: $Flash0" }
if (-not (Test-Path -LiteralPath $Flash1)) { throw "Secondary flash image not found: $Flash1" }

$b0 = [System.IO.File]::ReadAllBytes($Flash0)
$b1 = [System.IO.File]::ReadAllBytes($Flash1)
$comb = New-Object byte[] ($b0.Length + $b1.Length)
[Array]::Copy($b0, 0, $comb, 0, $b0.Length)
[Array]::Copy($b1, 0, $comb, $b0.Length, $b1.Length)
[System.IO.File]::WriteAllBytes($OutputPath, $comb)

$h = (Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant()
$invDir = Join-Path $EmuRoot 'build\generated'
New-Item -ItemType Directory -Force -Path $invDir | Out-Null
[ordered]@{
    schema_version = 'coinline.firmware_image/v1'
    kind           = 'flash0_plus_flash1'
    flash0         = @{ path = (Resolve-Path $Flash0).Path; size = $b0.Length }
    flash1         = @{ path = (Resolve-Path $Flash1).Path; size = $b1.Length }
    output         = @{ path = (Resolve-Path $OutputPath).Path; size = $comb.Length; sha256 = $h }
} | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $invDir 'firmware-image-inventory.json') -Encoding UTF8

Write-Host "Wrote $OutputPath size=$($comb.Length) sha256=$h"
exit 0
