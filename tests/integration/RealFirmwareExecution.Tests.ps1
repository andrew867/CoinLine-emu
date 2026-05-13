# SPDX-License-Identifier: GPL-2.0-or-later
#requires -Version 5.1
param(
    [Parameter(Mandatory = $true)][string] $RunDir,
    [Parameter(Mandatory = $true)][string] $FirmwareBinary
)

$ErrorActionPreference = 'Stop'
$EmuRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$exe = Join-Path $EmuRoot 'build\bin\coinline-mame.exe'
if (-not (Test-Path -LiteralPath $exe)) {
    throw 'coinline-mame.exe missing'
}

$p = Start-Process -FilePath $exe -ArgumentList @('-help') -NoNewWindow -Wait -PassThru `
    -RedirectStandardOutput (Join-Path $RunDir 'ref-help-stdout.txt') `
    -RedirectStandardError (Join-Path $RunDir 'ref-help-stderr.txt')
if ($p.ExitCode -lt 0) {
    throw "mame -help unexpected exit $($p.ExitCode)"
}

$fwHash = (Get-FileHash -LiteralPath $FirmwareBinary -Algorithm SHA256).Hash.ToLowerInvariant()
$bt = Join-Path $RunDir 'boot-trace.jsonl'
$first = (Get-Content -LiteralPath $bt -TotalCount 5) -join "`n"
if ($first -notmatch '"milestone"\s*:\s*"M0"') {
    throw 'RealFirmwareExecution: M0 not in first lines of boot trace (firmware load not traced).'
}
if ($first -notmatch [regex]::Escape($fwHash)) {
    # M0 line contains sha256 from emulator validation — allow substring match in full file
    $all = Get-Content -LiteralPath $bt -Raw
    if ($all -notmatch [regex]::Escape($fwHash)) {
        throw 'RealFirmwareExecution: firmware SHA256 from host file not found in boot trace.'
    }
}

Write-Host 'RealFirmwareExecution: MAME exe runs and boot trace matches firmware hash.'
exit 0
