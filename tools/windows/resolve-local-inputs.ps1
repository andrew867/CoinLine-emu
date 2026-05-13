# SPDX-License-Identifier: GPL-2.0-or-later
#requires -Version 5.1
<#
.SYNOPSIS
  Resolve firmware binary, firmware source root, MSYS2, MAME root; write build/local-inputs.json.
#>
param(
    [string] $RepoRoot = '',
    [string] $FirmwareHint = '../firmware/flash.bin',
    [string] $SourceHint = '..',
    [string] $MsysRoot = 'C:\msys64',
    [string] $MameRoot = 'third-party/mame',
    [string] $HostUrl = 'http://127.0.0.1:5000'
)

$ErrorActionPreference = 'Stop'
if (-not $RepoRoot) {
    $RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
}

function Resolve-PathSafe([string]$base, [string]$rel) {
    $cand = [System.IO.Path]::GetFullPath((Join-Path $base $rel))
    return $cand
}

$searched = [System.Collections.Generic.List[string]]::new()
$candidates = @(
    (Resolve-PathSafe $RepoRoot $FirmwareHint),
    (Resolve-PathSafe (Join-Path $RepoRoot '..') 'firmware/flash.bin'),
    (Join-Path $RepoRoot 'fixtures/firmware/flash.bin')
)
$firmware = ''
foreach ($c in $candidates) {
    $searched.Add($c)
    if (Test-Path -LiteralPath $c) {
        $firmware = $c
        break
    }
}
if (-not $firmware) {
    $bins = Get-ChildItem -Path (Join-Path $RepoRoot '..') -Recurse -File -Include flash.bin -ErrorAction SilentlyContinue | Select-Object -First 5
    foreach ($b in $bins) {
        $searched.Add($b.FullName)
        if ($b.Name -eq 'flash.bin') {
            $firmware = $b.FullName
            break
        }
    }
}

$srcCandidates = @(
    (Resolve-PathSafe $RepoRoot $SourceHint),
    (Resolve-PathSafe $RepoRoot '../firmware'),
    (Resolve-PathSafe $RepoRoot '..')
)
$fwsrc = ''
foreach ($s in $srcCandidates) {
    if (Test-Path -LiteralPath $s) {
        $fwsrc = $s
        break
    }
}

$mameAbs = if ([System.IO.Path]::IsPathRooted($MameRoot)) { $MameRoot } else { [System.IO.Path]::GetFullPath((Join-Path $RepoRoot $MameRoot)) }

$fwExists = [bool]($firmware -and (Test-Path -LiteralPath $firmware))
$size = 0L
$sha = ''
if ($fwExists) {
    $fi = Get-Item -LiteralPath $firmware
    $size = $fi.Length
    $sha = (Get-FileHash -LiteralPath $firmware -Algorithm SHA256).Hash.ToLowerInvariant()
}

$out = [ordered]@{
    resolved_firmware_binary = $firmware
    firmware_binary_exists   = $fwExists
    firmware_binary_size     = $size
    firmware_binary_sha256   = $sha
    resolved_firmware_source_root = $fwsrc
    firmware_source_exists   = [bool]($fwsrc -and (Test-Path -LiteralPath $fwsrc))
    msys_root                  = $MsysRoot
    msys_root_exists           = (Test-Path -LiteralPath $MsysRoot)
    mame_root                  = $mameAbs
    coinline_host_url          = $HostUrl
    timestamp_utc              = (Get-Date).ToUniversalTime().ToString('o')
    paths_searched             = @($searched)
}

$build = Join-Path $RepoRoot 'build'
New-Item -ItemType Directory -Force -Path $build | Out-Null
$jsonPath = Join-Path $build 'local-inputs.json'
$out | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $jsonPath -Encoding UTF8
Write-Host "Wrote $jsonPath"

if (-not $fwExists) {
    throw "Firmware binary not found after search. See paths_searched in $jsonPath"
}
exit 0
