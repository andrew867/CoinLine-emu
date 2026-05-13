# SPDX-License-Identifier: GPL-2.0-or-later
#requires -Version 5.1
param(
    [Parameter(Mandatory = $true)][string] $RunDir,
    [Parameter(Mandatory = $true)][string] $FirmwareBinary
)

$ErrorActionPreference = 'Stop'
$sum = Get-Content (Join-Path $RunDir 'run-summary.json') -Raw | ConvertFrom-Json
if ($sum.firmware -ne $FirmwareBinary) {
    throw 'run-summary firmware path mismatch'
}
if (-not (Test-Path -LiteralPath $sum.boot_trace)) {
    throw 'boot trace path from summary missing'
}
Write-Host 'FirmwareBootTraceIntegration: firmware path and trace consistent.'
exit 0
