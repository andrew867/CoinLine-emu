# SPDX-License-Identifier: GPL-2.0-or-later
#requires -Version 5.1
<#
.SYNOPSIS
  Thin launcher: run a single bash command in MSYS2 MINGW64 with coinline-emu as cwd.
  Do not put build logic in PowerShell — pass it as -Command.
#>
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string] $Command,

    [Parameter(Mandatory = $false)]
    [string] $RepoRoot
)

$ErrorActionPreference = 'Stop'
$Bash = 'C:\msys64\usr\bin\bash.exe'
if (-not (Test-Path -LiteralPath $Bash)) {
    throw "bash.exe not found: $Bash"
}

if (-not $RepoRoot) {
    $RepoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..')).Path
}

# Convert the Windows-style repo root into the form bash expects (e.g. /c/path/to/repo).
$unixRepoRoot = ($RepoRoot -replace '\\', '/')
if ($unixRepoRoot -match '^([A-Za-z]):/(.*)$') {
    $unixRepoRoot = "/$($Matches[1].ToLower())/$($Matches[2])"
}

# PATH must be literal for bash; escape $ so PowerShell does not expand it.
$bashLine = "export MSYSTEM=MINGW64; export CHERE_INVOKING=1; export PATH=/mingw64/bin:/usr/bin:`$PATH; cd '$unixRepoRoot' && set -euo pipefail && $Command"
& $Bash -lc $bashLine
exit $LASTEXITCODE
