# SPDX-License-Identifier: GPL-2.0-or-later
#requires -Version 5.1
<#
.SYNOPSIS
  Wrapper: MSYS2 bootstrap + MAME build (CoinLine driver overlay).

.NOTES
  Does not build CMake coinline_support tests — use CMake separately for those.
#>
$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot
& (Join-Path $here 'tools\windows\bootstrap-msys2-mame.ps1')
& (Join-Path $here 'tools\windows\build-mame-coinline.ps1')
Write-Host 'build.ps1: MAME track steps completed.'
