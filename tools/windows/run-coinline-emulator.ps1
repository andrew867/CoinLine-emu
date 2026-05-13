# SPDX-License-Identifier: GPL-2.0-or-later
#requires -Version 5.1
<#
.SYNOPSIS
  Run built MAME coinline target with real firmware, traces, and bounded execution.

.PARAMETER RunSeconds
  MAME -seconds_to_run (default 30).

.PARAMETER AllowIncomplete
  When true, validation of M10 is deferred to validate-boot-milestones.ps1 -AllowIncomplete.
#>
param(
    [string] $FirmwareBinary = '',
    [string] $MameExe = '',
    [int] $RunSeconds = 30,
    [string] $HostUrl = 'http://127.0.0.1:5000',
    [string] $BoardProfile = 'fixtures/board/board-profile-2line-vfd.json',
    [switch] $Windowed,
    [switch] $Debug,
    [switch] $Screenshot,
    [switch] $AllowIncomplete
)

$ErrorActionPreference = 'Stop'

$EmuRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not $FirmwareBinary) {
    if ($env:FIRMWARE_BINARY_PATH) { $FirmwareBinary = $env:FIRMWARE_BINARY_PATH }
}
if (-not $FirmwareBinary) {
    $inputs = Join-Path $EmuRoot 'build\local-inputs.json'
    if (Test-Path -LiteralPath $inputs) {
        $j = Get-Content -LiteralPath $inputs -Raw | ConvertFrom-Json
        if ($j.resolved_firmware_binary) { $FirmwareBinary = [string]$j.resolved_firmware_binary }
    }
}
if (-not $FirmwareBinary -or -not (Test-Path -LiteralPath $FirmwareBinary)) {
    throw 'FirmwareBinary required or missing. Run resolve-local-inputs.ps1 or pass -FirmwareBinary.'
}

if (-not $MameExe) {
    $def = Join-Path $EmuRoot 'build\bin\coinline-mame.exe'
    if (Test-Path -LiteralPath $def) { $MameExe = $def }
    elseif ($env:COINLINE_MAME_EXE) { $MameExe = $env:COINLINE_MAME_EXE }
}
if (-not $MameExe -or -not (Test-Path -LiteralPath $MameExe)) {
    throw "MAME executable not found. Build with build-mame-coinline.ps1 or set -MameExe / COINLINE_MAME_EXE."
}

$stamp = Get-Date -Format 'yyyyMMddTHHmmss'
$RunDir = Join-Path $EmuRoot "build\runs\$stamp"
New-Item -ItemType Directory -Force -Path $RunDir | Out-Null

$env:COINLINE_EMU_ROOT = $EmuRoot
$env:COINLINE_FIRMWARE = $FirmwareBinary
$env:COINLINE_BOARD = $BoardProfile
$env:COINLINE_HOST_URL = $HostUrl

$bootTrace = Join-Path $RunDir 'boot-trace.jsonl'
$ioTrace = Join-Path $RunDir 'io-trace.jsonl'
$unknownPort = Join-Path $RunDir 'unknown-port.jsonl'
$env:COINLINE_BOOT_TRACE = $bootTrace
$env:COINLINE_IO_TRACE = $ioTrace
$env:COINLINE_UNKNOWN_PORT_LOG = $unknownPort
$env:COINLINE_MILESTONE_JSON = Join-Path $RunDir 'milestones.json'

$audioTrace = Join-Path $RunDir 'audio-trace.jsonl'
$vwAudioTrace = Join-Path $RunDir 'voiceware-trace.jsonl'
$alerterTrace = Join-Path $RunDir 'alerter-trace.jsonl'
$supervisionTrace = Join-Path $RunDir 'supervision-trace.jsonl'
$env:COINLINE_AUDIO_TRACE = $audioTrace
$env:COINLINE_VOICEWARE_TRACE = $vwAudioTrace
$env:COINLINE_ALERTER_TRACE = $alerterTrace
$env:COINLINE_SUPERVISION_TRACE = $supervisionTrace
'' | Set-Content -LiteralPath $audioTrace -Encoding UTF8
'' | Set-Content -LiteralPath $vwAudioTrace -Encoding UTF8
'' | Set-Content -LiteralPath $alerterTrace -Encoding UTF8
'' | Set-Content -LiteralPath $supervisionTrace -Encoding UTF8

$video = if ($Windowed) { 'opengl' } else { 'none' }
$verbose = @()
if ($Debug) {
    $verbose = @('-verbose', '-log', '-oslog')
}

$mameArgs = @(
    'cl_millennium',
    '-rompath', (Split-Path -Parent $FirmwareBinary),
    '-video', $video,
    '-sound', 'none',
    '-nothrottle',
    '-seconds_to_run', "$RunSeconds"
) + $verbose

$runLog = Join-Path $RunDir 'run.log'
$stdoutPath = Join-Path $RunDir 'mame-stdout.log'
$stderrPath = Join-Path $RunDir 'mame-stderr.log'

$cmdLine = "$MameExe $($mameArgs -join ' ')"
@"
run-coinline-emulator.ps1
mame_exe=$MameExe
args=$($mameArgs -join ' ')
firmware=$FirmwareBinary
host_url=$HostUrl
run_seconds=$RunSeconds
"@ | Set-Content -LiteralPath $runLog -Encoding UTF8

$p = Start-Process -FilePath $MameExe -ArgumentList $mameArgs -WorkingDirectory $EmuRoot `
    -NoNewWindow -Wait -PassThru `
    -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath

Get-Content -LiteralPath $stdoutPath -ErrorAction SilentlyContinue | Add-Content -LiteralPath $runLog -Encoding UTF8
Add-Content -LiteralPath $runLog -Value '--- stderr ---' -Encoding UTF8
Get-Content -LiteralPath $stderrPath -ErrorAction SilentlyContinue | Add-Content -LiteralPath $runLog -Encoding UTF8

function Convert-JsonlFileToJsonArray([string]$Path, [string]$OutJson) {
    if (-not (Test-Path -LiteralPath $Path)) {
        '[]' | Set-Content -LiteralPath $OutJson -Encoding UTF8
        return
    }
    $arr = [System.Collections.Generic.List[object]]::new()
    Get-Content -LiteralPath $Path | ForEach-Object {
        $t = $_.Trim()
        if (-not $t) { return }
        try { $arr.Add(($t | ConvertFrom-Json)) } catch { }
    }
    ($arr | ConvertTo-Json -Depth 12) | Set-Content -LiteralPath $OutJson -Encoding UTF8
}

$bootMilestones = Join-Path $RunDir 'boot-milestones.json'
Convert-JsonlFileToJsonArray -Path $bootTrace -OutJson $bootMilestones

$ioLog = Join-Path $RunDir 'io-trace.log'
if (Test-Path -LiteralPath $ioTrace) {
    Copy-Item -LiteralPath $ioTrace -Destination $ioLog -Force
}
else {
    '' | Set-Content -LiteralPath $ioLog -Encoding UTF8
}

$unknownJson = Join-Path $RunDir 'unknown-ports.json'
Convert-JsonlFileToJsonArray -Path $unknownPort -OutJson $unknownJson

$vfdLog = Join-Path $RunDir 'vfd-trace.log'
$uartLog = Join-Path $RunDir 'uart-transcript.log'
$ioText = if (Test-Path -LiteralPath $ioTrace) { Get-Content -LiteralPath $ioTrace -Raw } else { '' }
if ($ioText -match 'vfd|VFD') {
    ($ioText -split "`n" | Where-Object { $_ -match 'vfd|VFD' }) -join "`n" | Set-Content -LiteralPath $vfdLog -Encoding UTF8
}
if ($ioText -match 'uart|UART|modem|MODEM') {
    ($ioText -split "`n" | Where-Object { $_ -match 'uart|UART|modem|MODEM' }) -join "`n" | Set-Content -LiteralPath $uartLog -Encoding UTF8
}

if ($Screenshot) {
    $cap = Join-Path $PSScriptRoot 'capture-mame-screenshot.ps1'
    if (Test-Path -LiteralPath $cap) {
        & $cap -RunDir $RunDir
    }
}

$summary = [ordered]@{
    exit_code          = $p.ExitCode
    mame_exe           = $MameExe
    machine            = 'cl_millennium'
    firmware           = $FirmwareBinary
    host_url           = $HostUrl
    run_seconds        = $RunSeconds
    run_dir            = $RunDir
    boot_trace         = $bootTrace
    boot_milestones_json = $bootMilestones
    io_trace           = $ioTrace
    io_trace_log       = $ioLog
    unknown_port_log   = $unknownPort
    unknown_ports_json = $unknownJson
    milestone_json     = $env:COINLINE_MILESTONE_JSON
    allow_incomplete   = [bool]$AllowIncomplete
    command_line       = $cmdLine
}
$summary | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $RunDir 'run-summary.json') -Encoding UTF8

if (-not (Test-Path -LiteralPath $bootTrace)) {
    throw "Boot trace missing: $bootTrace (MAME did not run driver start)."
}

if ($p.ExitCode -ne 0) {
    throw "MAME exited with code $($p.ExitCode). Logs: $RunDir"
}

Write-Host "Run complete. Artifacts: $RunDir"
exit 0
