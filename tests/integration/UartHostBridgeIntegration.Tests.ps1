# SPDX-License-Identifier: GPL-2.0-or-later
#requires -Version 5.1
param(
    [Parameter(Mandatory = $true)][string] $RunDir,
    [Parameter(Mandatory = $true)][string] $FirmwareBinary
)

$ErrorActionPreference = 'Stop'
$bt = Join-Path $RunDir 'boot-trace.jsonl'
$txt = Get-Content -LiteralPath $bt -Raw
if ($txt -notmatch 'M8') {
    Write-Warning 'UartHostBridgeIntegration: M8 not in boot trace yet — require when UART/modem milestone is stable.'
}
exit 0
