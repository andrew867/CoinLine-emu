# SPDX-License-Identifier: GPL-2.0-or-later
#requires -Version 5.1
param(
    [Parameter(Mandatory = $true)][string] $RunDir,
    [Parameter(Mandatory = $true)][string] $FirmwareBinary
)

$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath $FirmwareBinary)) {
    throw "Firmware missing: $FirmwareBinary"
}
$bt = Join-Path $RunDir 'boot-trace.jsonl'
if (-not (Test-Path -LiteralPath $bt)) {
    throw "boot-trace.jsonl missing under $RunDir — MAME did not emit boot trace."
}
$summary = Join-Path $RunDir 'run-summary.json'
if (-not (Test-Path -LiteralPath $summary)) {
    throw "run-summary.json missing — run-coinline-emulator.ps1 did not complete."
}
Write-Host 'BootMilestoneIntegration: boot trace and run summary present.'
exit 0
