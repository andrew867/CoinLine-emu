# SPDX-License-Identifier: GPL-2.0-or-later
#requires -Version 5.1
param(
    [string] $U16 = '..\firmware\voice_a.bin',
    [string] $U26 = '..\firmware\voice_b.bin'
)
$ErrorActionPreference = 'Stop'
$EmuRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$exe = Join-Path $EmuRoot 'build\bin\extract-voiceware-rom.exe'
if (-not (Test-Path $exe)) {
    Write-Warning "Build extract-voiceware-rom via CMake first."
    exit 2
}
$gen = Join-Path $EmuRoot 'build\generated'
New-Item -ItemType Directory -Force -Path $gen | Out-Null
$o = @()
$o += & $exe $U16
$o += & $exe $U26
$txt = $o -join "`n"
Set-Content (Join-Path $gen 'voiceware-extraction-report.json') -Value $txt -Encoding UTF8
Write-Host $txt

