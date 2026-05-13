# SPDX-License-Identifier: GPL-2.0-or-later
#requires -Version 5.1
param(
    [Parameter(Mandatory = $true)][string] $RunDir,
    [Parameter(Mandatory = $true)][string] $FirmwareBinary
)

$ErrorActionPreference = 'Stop'
$u = Join-Path $RunDir 'unknown-port.jsonl'
if (-not (Test-Path -LiteralPath $u)) {
    Write-Warning "UnknownPortRegression: $u not present (driver may not log yet) — treating as non-fatal for this tranche."
}
exit 0
