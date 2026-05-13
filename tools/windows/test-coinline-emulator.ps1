# SPDX-License-Identifier: GPL-2.0-or-later
#requires -Version 5.1
<#
.SYNOPSIS
  Resolve inputs, ensure MSYS2+MAME, build if needed, run real firmware, validate milestones, run shell-out integration tests.
#>
param(
    [string] $FirmwareBinary = '',
    [string] $MsysRoot = 'C:\msys64',
    [string] $MameRoot = '',
    [switch] $SkipBootstrap,
    [switch] $SkipIfNoFirmware,
    [switch] $AllowIncompleteM10,
    [int] $RunSeconds = 30
)

$ErrorActionPreference = 'Stop'
$EmuRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path

& (Join-Path $PSScriptRoot 'resolve-local-inputs.ps1') -MsysRoot $MsysRoot
if ($LASTEXITCODE -ne 0) { throw 'resolve-local-inputs.ps1 failed' }

$inputs = Get-Content (Join-Path $EmuRoot 'build\local-inputs.json') -Raw | ConvertFrom-Json
if (-not $FirmwareBinary) {
    $FirmwareBinary = [string]$inputs.resolved_firmware_binary
}
if (-not $FirmwareBinary -or -not (Test-Path -LiteralPath $FirmwareBinary)) {
    if ($SkipIfNoFirmware) {
        Write-Warning 'Firmware missing — exit 77.'
        exit 77
    }
    throw 'Real firmware path required.'
}

if (-not $MameRoot) {
    $MameRoot = [string]$inputs.mame_root
}
if (-not (Test-Path -LiteralPath $MsysRoot)) {
    throw "MSYS2 not found: $MsysRoot"
}
if (-not (Test-Path -LiteralPath $MameRoot)) {
    throw "MAME root not found: $MameRoot — run bootstrap-msys2-mame.ps1"
}

if (-not $SkipBootstrap) {
    & (Join-Path $PSScriptRoot 'bootstrap-msys2-mame.ps1') -MsysRoot $MsysRoot -MameRoot $MameRoot
    if ($LASTEXITCODE -ne 0) { throw 'bootstrap-msys2-mame.ps1 failed' }
}

$exe = Join-Path $EmuRoot 'build\bin\coinline-mame.exe'
if (-not (Test-Path -LiteralPath $exe)) {
    & (Join-Path $PSScriptRoot 'build-mame-coinline.ps1') -MsysRoot $MsysRoot -MameRoot $MameRoot
    if ($LASTEXITCODE -ne 0) { throw 'build-mame-coinline.ps1 failed' }
}

$run = Join-Path $PSScriptRoot 'run-coinline-emulator.ps1'
$hostUrl = if ($inputs.coinline_host_url) { [string]$inputs.coinline_host_url } else { 'http://127.0.0.1:5000' }
& $run -FirmwareBinary $FirmwareBinary -RunSeconds $RunSeconds -HostUrl $hostUrl
if ($LASTEXITCODE -ne 0) { throw "run-coinline-emulator.ps1 failed ($LASTEXITCODE)" }

$latest = Get-ChildItem -LiteralPath (Join-Path $EmuRoot 'build\runs') -Directory -ErrorAction SilentlyContinue |
    Sort-Object Name -Descending | Select-Object -First 1
if (-not $latest) { throw 'No run directory under build/runs' }
$runDir = $latest.FullName

$val = Join-Path $PSScriptRoot 'validate-boot-milestones.ps1'
if ($AllowIncompleteM10) {
    & $val -RunDir $runDir -AllowIncomplete
}
else {
    & $val -RunDir $runDir
}
$vex = $LASTEXITCODE
if ($vex -ne 0 -and -not $AllowIncompleteM10) {
    throw "validate-boot-milestones.ps1 failed ($vex)"
}

$tests = @(
    (Join-Path $EmuRoot 'tests\integration\BootMilestoneIntegration.Tests.ps1'),
    (Join-Path $EmuRoot 'tests\integration\RealFirmwareExecution.Tests.ps1'),
    (Join-Path $EmuRoot 'tests\integration\FirmwareBootTraceIntegration.Tests.ps1'),
    (Join-Path $EmuRoot 'tests\integration\UnknownPortRegression.Tests.ps1'),
    (Join-Path $EmuRoot 'tests\integration\UnknownPortReport.Tests.ps1'),
    (Join-Path $EmuRoot 'tests\integration\VfdBootOutputIntegration.Tests.ps1'),
    (Join-Path $EmuRoot 'tests\integration\VfdFirmwareDriven.Tests.ps1'),
    (Join-Path $EmuRoot 'tests\integration\UartHostBridgeIntegration.Tests.ps1')
)
foreach ($t in $tests) {
    if (-not (Test-Path -LiteralPath $t)) {
        throw "Missing integration test file: $t"
    }
    Write-Host "Running $t ..."
    & $t -RunDir $runDir -FirmwareBinary $FirmwareBinary
    if ($LASTEXITCODE -ne 0) {
        throw "Integration test failed: $t ($LASTEXITCODE)"
    }
}

Write-Host 'test-coinline-emulator.ps1: pipeline OK.'
exit 0
