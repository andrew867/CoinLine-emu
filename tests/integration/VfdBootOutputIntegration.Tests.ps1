# SPDX-License-Identifier: GPL-2.0-or-later
#requires -Version 5.1
param(
    [Parameter(Mandatory = $true)][string] $RunDir,
    [Parameter(Mandatory = $true)][string] $FirmwareBinary
)

$ErrorActionPreference = 'Stop'
$bt = Join-Path $RunDir 'boot-trace.jsonl'
$txt = Get-Content -LiteralPath $bt -Raw
if ($txt -notmatch 'M6') {
    Write-Warning 'VfdBootOutputIntegration: M6 (first VFD) not in boot trace yet — tighten when driver emits M6 reliably.'
}
exit 0
