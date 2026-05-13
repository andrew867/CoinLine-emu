# SPDX-License-Identifier: GPL-2.0-or-later
#requires -Version 5.1
# Alias-style suite name (UnknownPortReport*) — shells out only via test-coinline-emulator pipeline.
param(
    [Parameter(Mandatory = $true)][string] $RunDir,
    [Parameter(Mandatory = $true)][string] $FirmwareBinary
)

$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
& (Join-Path $here 'UnknownPortRegression.Tests.ps1') -RunDir $RunDir -FirmwareBinary $FirmwareBinary
exit $LASTEXITCODE
