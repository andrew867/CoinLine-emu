# SPDX-License-Identifier: GPL-2.0-or-later
#requires -Version 5.1
<#
.SYNOPSIS
  MSYS2 toolchain + MAME clone/update, pinned checkout, environment JSON and logs.

.PARAMETER MsysRoot
  MSYS2 root (default C:\msys64).

.PARAMETER MameRoot
  MAME clone root (default: <coinline-emu>/third-party/mame).

.PARAMETER ShellFlavor
  UCRT64 | CLANG64 | MINGW64 | MSYS2 — passed to msys2_shell.cmd.

.PARAMETER SkipPacman
  Skip pacman (packages already installed).

.NOTES
  Logs every step to build/logs/bootstrap-msys2-mame.log.
  Writes build/msys2-environment.json and build/mame-version.json.
#>
param(
    [string] $MsysRoot = 'C:\msys64',
    [string] $MameRoot = '',
    [ValidateSet('UCRT64', 'CLANG64', 'MINGW64', 'MSYS2')]
    [string] $ShellFlavor = 'MINGW64',
    [switch] $SkipPacman
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

function Get-EmuRoot {
    return (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
}

function Write-Log {
    param([string] $Message)
    $line = "$(Get-Date -Format 'o')  $Message"
    Write-Host $line
    if ($script:LogFile) {
        Add-Content -LiteralPath $script:LogFile -Value $line -Encoding UTF8
    }
}

$EmuRoot = Get-EmuRoot
$BuildDir = Join-Path $EmuRoot 'build'
$LogDir = Join-Path $BuildDir 'logs'
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
$script:LogFile = Join-Path $LogDir 'bootstrap-msys2-mame.log'
Write-Log "bootstrap-msys2-mame.ps1 start EmuRoot=$EmuRoot MsysRoot=$MsysRoot ShellFlavor=$ShellFlavor"

if (-not $MameRoot) {
    $MameRoot = Join-Path $EmuRoot 'third-party\mame'
}

if (-not (Test-Path -LiteralPath $MsysRoot)) {
    Write-Log "ERROR: MSYS2 not found: $MsysRoot"
    throw "MSYS2 not found at $MsysRoot"
}

$launchers = @(
    @{ Name = 'UCRT64'; Path = Join-Path $MsysRoot 'ucrt64.exe' },
    @{ Name = 'CLANG64'; Path = Join-Path $MsysRoot 'clang64.exe' },
    @{ Name = 'MINGW64'; Path = Join-Path $MsysRoot 'mingw64.exe' },
    @{ Name = 'MSYS2'; Path = Join-Path $MsysRoot 'msys2.exe' }
)
$chosen = $launchers | Where-Object { $_.Name -eq $ShellFlavor } | Select-Object -First 1
if (-not (Test-Path -LiteralPath $chosen.Path)) {
    foreach ($c in $launchers) {
        if (Test-Path -LiteralPath $c.Path) {
            Write-Log "Shell $ShellFlavor missing; falling back to $($c.Name)"
            $ShellFlavor = $c.Name
            $chosen = $c
            break
        }
    }
}
if (-not (Test-Path -LiteralPath $chosen.Path)) {
    throw "No MSYS2 shell launcher found under $MsysRoot"
}

$Bash = Join-Path $MsysRoot 'usr\bin\bash.exe'
if (-not (Test-Path -LiteralPath $Bash)) {
    throw "bash.exe missing at $Bash"
}

$ShellCmd = Join-Path $MsysRoot 'msys2_shell.cmd'
if (-not (Test-Path -LiteralPath $ShellCmd)) {
    throw "Missing $ShellCmd"
}

$shellArg = switch ($ShellFlavor) {
    'UCRT64' { '-ucrt64' }
    'CLANG64' { '-clang64' }
    'MINGW64' { '-mingw64' }
    'MSYS2' { '-msys' }
    default { '-ucrt64' }
}

# MAME Windows build uses MSYSTEM=MINGW64 (see build-mame-coinline.ps1); use mingw-w64-x86_64-* packages.
$Pkgs = @(
    'git', 'make', 'diffutils', 'patch', 'unzip', 'tar', 'zip',
    'pkgconf', 'python',
    'mingw-w64-x86_64-toolchain',
    'mingw-w64-x86_64-python',
    'mingw-w64-x86_64-SDL2',
    'mingw-w64-x86_64-SDL2_ttf',
    'mingw-w64-x86_64-qt6-base'
)

if (-not $SkipPacman) {
    $pkgLine = ($Pkgs -join ' ')
    $pacmanInner = @"
set -euo pipefail
export MSYS2_ARG_CONV_EXCL='*'
echo '=== pacman -Sy (sync DBs; avoid -Syu here — it may require closing shells) ==='
pacman -Sy --noconfirm
echo '=== pacman -S packages ==='
pacman -S --needed --noconfirm $pkgLine
"@
    Write-Log "Running pacman via msys2_shell (long-running)..."
    $pacFile = Join-Path $BuildDir '_bootstrap_pacman_inner.sh'
    [System.IO.File]::WriteAllText($pacFile, $pacmanInner, [System.Text.UTF8Encoding]::new($false))
    $CygpathExe = Join-Path $MsysRoot 'usr\bin\cygpath.exe'
    if (-not (Test-Path -LiteralPath $CygpathExe)) {
        throw "cygpath.exe not found: $CygpathExe"
    }
    $pacUnix = (& $CygpathExe @('-u', $pacFile)).Trim()
    Write-Log "inner script: $pacUnix"
    # Pacman prints "skipping" notices to stderr; with $ErrorActionPreference Stop, 2>&1 becomes ErrorRecords and aborts.
    $prevErrAct = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & $ShellCmd @('-defterm', '-here', '-no-start', $shellArg, '-c', "exec bash -euo pipefail `"$pacUnix`"") 2>&1 | ForEach-Object {
            if ($_ -is [System.Management.Automation.ErrorRecord]) {
                Write-Log $_.Exception.Message
            }
            else {
                Write-Log $_
            }
        }
    }
    finally {
        $ErrorActionPreference = $prevErrAct
    }
    if ($LASTEXITCODE -ne 0) {
        throw "pacman step failed exit $LASTEXITCODE"
    }
}
else {
    Write-Log 'SkipPacman: not installing packages.'
}

$pkgQuery = & $Bash -lc "pacman -Q $($Pkgs -join ' ') 2>&1" | Out-String
$envJson = [ordered]@{
    msys_root      = $MsysRoot
    shell_flavor   = $ShellFlavor
    shell_arg      = $shellArg
    packages_requested = $Pkgs
    pacman_query_tail  = ($pkgQuery.Trim().Split("`n") | Select-Object -Last 40) -join "`n"
    skip_pacman    = [bool]$SkipPacman
    timestamp_utc  = (Get-Date).ToUniversalTime().ToString('o')
}
$envJson | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $BuildDir 'msys2-environment.json') -Encoding UTF8
Write-Log 'Wrote build/msys2-environment.json'

try {
    $git = Get-Command git -ErrorAction Stop
}
catch {
    throw 'git not on PATH.'
}

$MameParent = Split-Path -Parent $MameRoot
New-Item -ItemType Directory -Force -Path $MameParent | Out-Null

$pinnedRef = ''
$pinFile = Join-Path $EmuRoot 'config\mame-pinned.ref'
if (Test-Path -LiteralPath $pinFile) {
    $pinnedRef = (Get-Content -LiteralPath $pinFile | Where-Object { $_ -match '\S' -and $_ -notmatch '^\s*#' } | Select-Object -First 1).Trim()
}

$frozenCommit = ''
$mameVerPath = Join-Path $BuildDir 'mame-version.json'
if (Test-Path -LiteralPath $mameVerPath) {
    try {
        $prev = Get-Content -LiteralPath $mameVerPath -Raw | ConvertFrom-Json
        if ($prev.commit) { $frozenCommit = [string]$prev.commit }
    }
    catch { }
}

if (-not (Test-Path -LiteralPath $MameRoot)) {
    Write-Log "git clone -> $MameRoot"
    & $git.Source clone 'https://github.com/mamedev/mame.git' $MameRoot
    if ($LASTEXITCODE -ne 0) { throw "git clone failed $LASTEXITCODE" }
}

Push-Location $MameRoot
try {
    Write-Log 'git fetch --tags --prune origin'
    & $git.Source fetch --tags --prune origin
    if ($LASTEXITCODE -ne 0) { throw "git fetch failed $LASTEXITCODE" }

    if ($frozenCommit) {
        Write-Log "checkout frozen commit $frozenCommit"
        & $git.Source checkout $frozenCommit
        if ($LASTEXITCODE -ne 0) { throw "git checkout $frozenCommit failed" }
    }
    elseif ($pinnedRef) {
        Write-Log "checkout pinned ref $pinnedRef"
        & $git.Source checkout $pinnedRef
        if ($LASTEXITCODE -ne 0) { throw "git checkout $pinnedRef failed" }
    }
    else {
        Write-Log 'no pin/freeze; staying on current branch'
    }

    $Commit = (& $git.Source rev-parse HEAD).Trim()
    $Branch = (& $git.Source rev-parse --abbrev-ref HEAD).Trim()
    $Remote = ''
    try {
        $Remote = (& $git.Source remote get-url origin).Trim()
    }
    catch { $Remote = 'unknown' }
}
finally {
    Pop-Location
}

$verOut = [ordered]@{
    root          = $MameRoot
    commit        = $Commit
    branch        = $Branch
    remote_url    = $Remote
    pinned_ref    = $pinnedRef
    frozen_commit = $frozenCommit
    timestamp_utc = (Get-Date).ToUniversalTime().ToString('o')
}
$verOut | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $mameVerPath -Encoding UTF8
Write-Log "Wrote $mameVerPath commit=$Commit"
Write-Log 'bootstrap-msys2-mame.ps1 finished OK.'
exit 0
