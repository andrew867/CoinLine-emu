# SPDX-License-Identifier: GPL-2.0-or-later
# Build helper for the MAME engine track (Windows / PowerShell).
# Requires EXTERNAL_MAME_ROOT to point at a MAME source checkout.

param(
    [switch]$Help
)

if ($Help) {
    Get-Content $PSCommandPath | Select-Object -First 18 | Select-Object -Skip 1
    exit 0
}

if (-not $env:EXTERNAL_MAME_ROOT) {
    Write-Host "EXTERNAL_MAME_ROOT is not set. Export it to your MAME checkout, then re-run. (No build performed.)" -ForegroundColor Yellow
    exit 0
}

if (-not (Test-Path -LiteralPath $env:EXTERNAL_MAME_ROOT -PathType Container)) {
    Write-Error "EXTERNAL_MAME_ROOT is not a directory: $($env:EXTERNAL_MAME_ROOT)"
    exit 1
}

$jobs = $env:MAME_MAKE_JOBS
if (-not $jobs) { $jobs = 4 }

Write-Host "build-mame.ps1: using MAME tree: $($env:EXTERNAL_MAME_ROOT)"
Write-Host "build-mame.ps1: NOTE — coinline subtarget registration lands in tranche E2+; this invokes default make."

Push-Location $env:EXTERNAL_MAME_ROOT
try {
    & make -j$jobs
} finally {
    Pop-Location
}
