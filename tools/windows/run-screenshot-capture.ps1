# SPDX-License-Identifier: GPL-2.0-or-later
#requires -Version 5.1
<#
.SYNOPSIS
  Run coinline-mame with real firmware, emit traces, capture GUI screenshots during the run, write evidence artifacts.

.PARAMETER Headless
  Use -video none (no window). Overrides -Windowed.

.PARAMETER Windowed
   When true (default), uses OpenGL window for real GUI screenshots.

.PARAMETER Screenshot
   Reserved for CI/scripts that pass `-Screenshot`; GUI capture always runs when `-Windowed` and not `-Headless`. Snapshot directory is always set under the run folder.

.NOTES
  Screenshots use CopyFromScreen while the MAME window is alive (async process).
  COINLINE_IO_TRACE is set for parity with run-coinline-emulator.ps1; the driver may not emit io-trace.jsonl yet - see evidence report.
#>
param(
    [Parameter(Mandatory = $true)][string] $FirmwareBinary,
    [string] $FirmwareSourceRoot = '',
    [string] $HostUrl = 'http://127.0.0.1:5000',
    [int] $RunSeconds = 180,
    # Microwire JSONL idle early-exit (see .SYNOPSIS in run-craft-nvram-two-phase.sh). 0 = disabled.
    [int] $MicrowireEarlyStopIdleSeconds = 0,
    [int] $MicrowireEarlyStopMinJsonlLines = 8,
    [int] $MicrowireEarlyStopWarmSeconds = 4,
    [int] $MicrowireEarlyStopPollMilliseconds = 500,
    # When true, Microwire JSONL idle early-exit does not trigger until at least one committed
    # "write" op appears in the trace (avoids stopping after read-only EEPROM traffic).
    [bool] $MicrowireEarlyStopRequireCommittedWrite = $false,
    [Parameter(Mandatory = $true)][string] $OutputDir,
    [bool] $Windowed = $true,
    [switch] $Headless,
    [switch] $Screenshot,
    [switch] $MameVerbose,
    [int] $CaptureIntervalMs = 1000,
    [string] $MameExe = '',
    [string] $BoardProfile = 'fixtures/board/board-profile-2line-vfd.json',
    [string] $VoiceRomU16 = '../firmware/voice_a.bin',
    [string] $VoiceRomU26 = '../firmware/voice_b.bin',
    [ValidateSet('banked_two_roms', 'u16_u26_concat', 'u26_u16_concat', 'u16_only', 'u26_only')]
    [string] $VoiceRomLayout = 'banked_two_roms',
    [switch] $EnableAudio,
    [switch] $WavWrite,
    [switch] $AudioTrace,
    [switch] $VoicewareTrace,
    [switch] $RealInputDemo,
    [ValidateSet('fast', 'm6', 'uart', 'voice', 'full', 'tp_timing')]
    [string] $TraceProfile = 'fast',
    # Per-run MAME NVRAM directory (battery-backed device save). Default: <OutputDir>/nvram
    [string] $NvramDirectory = '',
    # First-boot JSON image (COINLINE_NVRAM). Omit on second boot when .nv already exists.
    [string] $InitialNvramJson = ''
)

$ErrorActionPreference = 'Stop'
$EmuRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if ($WavWrite) { $EnableAudio = $true }
if (-not (Test-Path -LiteralPath $FirmwareBinary)) {
    throw "Firmware not found: $FirmwareBinary"
}
if (-not $MameExe) {
    $def = Join-Path $EmuRoot 'build\bin\coinline-mame.exe'
    if (Test-Path -LiteralPath $def) { $MameExe = $def }
}
if (-not $MameExe -or -not (Test-Path -LiteralPath $MameExe)) {
    throw "Build coinline-mame.exe first (build-mame-coinline.ps1)"
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

# EEPROM/NVRAM evidence runs need microwire + nvram-storage JSONL. A user-level Windows
# COINLINE_TRACE_ONLY can land in this process even when the parent bash session unset it.
if ($env:COINLINE_UNSET_TRACE_ONLY -eq '1') {
    Remove-Item Env:\COINLINE_TRACE_ONLY -ErrorAction SilentlyContinue
    [Environment]::SetEnvironmentVariable('COINLINE_TRACE_ONLY', $null, 'Process')
}

function Get-CoinlineSha256([string]$Path) {
    if (Get-Command Get-FileHash -ErrorAction SilentlyContinue) {
        return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    $stream = [System.IO.File]::OpenRead($Path)
    try {
        $sha = [System.Security.Cryptography.SHA256]::Create()
        try {
            $bytes = $sha.ComputeHash($stream)
            return (($bytes | ForEach-Object { $_.ToString('x2') }) -join '')
        }
        finally {
            $sha.Dispose()
        }
    }
    finally {
        $stream.Dispose()
    }
}

# Voiceware ROM staging (names must match src/mame/coinline/millennium.cpp ROM_LOAD).
# MAME resolves `-rompath <dir>` by shortname (cl_millennium): ROM files live under <dir>/<shortname>/.
$runRomPath = Join-Path $OutputDir 'mame-rompath'
$mameShortNameDir = Join-Path $runRomPath 'cl_millennium'
New-Item -ItemType Directory -Force -Path $mameShortNameDir | Out-Null
$genDir = Join-Path $EmuRoot 'build\generated'
New-Item -ItemType Directory -Force -Path $genDir | Out-Null
if ($VoiceRomLayout -ne 'banked_two_roms') {
    $msg = "VoiceRomLayout '$VoiceRomLayout' is not supported for cl_millennium ROM_START (requires two ROM_LOAD entries). Use banked_two_roms."
    @{ status = 'unsupported_layout'; error = $msg; requested = $VoiceRomLayout } | ConvertTo-Json | Set-Content (Join-Path $OutputDir 'voiceware-layout-error.json') -Encoding UTF8
    throw $msg
}
if (-not (Test-Path -LiteralPath $VoiceRomU16)) { throw "VoiceRomU16 not found: $VoiceRomU16" }
if (-not (Test-Path -LiteralPath $VoiceRomU26)) { throw "VoiceRomU26 not found: $VoiceRomU26" }
Copy-Item -LiteralPath $VoiceRomU16 -Destination (Join-Path $mameShortNameDir 'voice_a.bin') -Force
Copy-Item -LiteralPath $VoiceRomU26 -Destination (Join-Path $mameShortNameDir 'voice_b.bin') -Force
$i16 = Get-Item -LiteralPath $VoiceRomU16
$i26 = Get-Item -LiteralPath $VoiceRomU26
$h16 = Get-CoinlineSha256 $VoiceRomU16
$h26 = Get-CoinlineSha256 $VoiceRomU26
$voicewareRomsDoc = [ordered]@{
    schema_version = 'coinline.voiceware_roms/v1'
    layout         = $VoiceRomLayout
    u16            = @{ path = $VoiceRomU16; size = $i16.Length; sha256 = $h16; staged_as = 'cl_millennium/voice_a.bin' }
    u26            = @{ path = $VoiceRomU26; size = $i26.Length; sha256 = $h26; staged_as = 'cl_millennium/voice_b.bin' }
    mame_rompath   = $runRomPath
    mame_set_dir   = $mameShortNameDir
}
$voicewareRomsDoc | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $OutputDir 'voiceware-roms.json') -Encoding UTF8
$voicewareRomsDoc | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $genDir 'voiceware-rom-inventory.json') -Encoding UTF8
[ordered]@{
    schema_version             = 'coinline.voicew_region/v1'
    voicew_region_size_bytes   = 0x200000
    rom_load_order             = @('voice_a.bin@0x000000', 'voice_b.bin@0x100000')
    banked_two_roms_interpretation = '16 banks: banks 0-7 map to U16, banks 8-15 map to U26 (128 KiB each)'
} | ConvertTo-Json | Set-Content (Join-Path $OutputDir 'voiceware-region-report.json') -Encoding UTF8

$guiUsed = -not $Headless

$fwItem = Get-Item -LiteralPath $FirmwareBinary
$fwSha = Get-CoinlineSha256 $FirmwareBinary

$inputRes = [ordered]@{
    timestamp                     = (Get-Date -Format 'yyyyMMddTHHmmss')
    firmware_binary_absolute      = (Resolve-Path $FirmwareBinary).Path
    firmware_binary_size          = $fwItem.Length
    firmware_sha256               = $fwSha
    firmware_source_root_absolute = if ($FirmwareSourceRoot) { (Resolve-Path $FirmwareSourceRoot).Path } else { '' }
    msys_root                     = 'C:\msys64'
    coinline_host_url             = $HostUrl
    coinline_emu_root             = $EmuRoot
}
$inputRes | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $OutputDir 'input-resolution.json') -Encoding UTF8

$env:COINLINE_EMU_ROOT = $EmuRoot
$env:COINLINE_FIRMWARE = $FirmwareBinary
$env:COINLINE_BOARD = $BoardProfile
$env:COINLINE_HOST_URL = $HostUrl
$env:COINLINE_TRACE_PROFILE = $TraceProfile
$env:COINLINE_TRACE_VFD = '1'

$bootTrace = Join-Path $OutputDir 'boot-trace.jsonl'
$ioTrace = Join-Path $OutputDir 'io-trace.jsonl'
$unknownPort = Join-Path $OutputDir 'unknown-port.jsonl'
$memTrace = Join-Path $OutputDir 'memory-trace.jsonl'
$cpuTrace = Join-Path $OutputDir 'cpu-trace.jsonl'
$z180Trace = Join-Path $OutputDir 'z180-register-trace.jsonl'

# Pin Microwire / NVRAM JSONL paths to this OutputDir when unset or when MSYS exported a
# Unix path string. coinline-mame.exe resolves these via the Win32 environment block; fixing
# here ensures Start-Process inherits paths std::ofstream can open on Windows.
function Set-CoinlinePinnedJsonlEnv([string]$VarName, [string]$LeafName) {
    $cur = [Environment]::GetEnvironmentVariable($VarName, 'Process')
    $target = [System.IO.Path]::GetFullPath((Join-Path $OutputDir $LeafName))
    if (-not $cur -or $cur.Trim() -eq '' -or $cur -match '^/[a-zA-Z]/') {
        [Environment]::SetEnvironmentVariable($VarName, $target, 'Process')
        return
    }
    try {
        $full = [System.IO.Path]::GetFullPath($cur)
        [Environment]::SetEnvironmentVariable($VarName, $full, 'Process')
    }
    catch {
        [Environment]::SetEnvironmentVariable($VarName, $target, 'Process')
    }
}
Set-CoinlinePinnedJsonlEnv 'COINLINE_MICROWIRE_TRACE' 'microwire-eeprom-trace.jsonl'
Set-CoinlinePinnedJsonlEnv 'COINLINE_NVRAM_STORAGE_TRACE' 'nvram-storage-trace.jsonl'
[ordered]@{
    COINLINE_MICROWIRE_TRACE       = [Environment]::GetEnvironmentVariable('COINLINE_MICROWIRE_TRACE', 'Process')
    COINLINE_NVRAM_STORAGE_TRACE   = [Environment]::GetEnvironmentVariable('COINLINE_NVRAM_STORAGE_TRACE', 'Process')
    COINLINE_TRACE_MICROWIRE       = [Environment]::GetEnvironmentVariable('COINLINE_TRACE_MICROWIRE', 'Process')
    COINLINE_TRACE_NVRAM_STORAGE   = [Environment]::GetEnvironmentVariable('COINLINE_TRACE_NVRAM_STORAGE', 'Process')
    COINLINE_TRACE_ONLY            = [Environment]::GetEnvironmentVariable('COINLINE_TRACE_ONLY', 'Process')
    COINLINE_UNSET_TRACE_ONLY      = [Environment]::GetEnvironmentVariable('COINLINE_UNSET_TRACE_ONLY', 'Process')
} | ConvertTo-Json | Set-Content (Join-Path $OutputDir 'sidecar-trace-env.json') -Encoding UTF8

$fullProfile = ($TraceProfile -eq 'full')
$m6Profile = ($TraceProfile -eq 'm6')
$uartProfile = ($TraceProfile -eq 'uart')
$voiceProfile = ($TraceProfile -eq 'voice')
$tpTimingProfile = ($TraceProfile -eq 'tp_timing')
if ($fullProfile -and $env:COINLINE_TRACE_STACK -eq '1') {
    $env:COINLINE_STACK_TRACE = (Join-Path $OutputDir 'stack-trace.jsonl')
}
if ($fullProfile -and $env:COINLINE_TRACE_RAM_INIT -eq '1') {
    $env:COINLINE_RAM_INIT_TRACE = (Join-Path $OutputDir 'ram-init-trace.jsonl')
}
if ($fullProfile -and $env:COINLINE_TRACE_MMU_TRANSLATION -eq '1') {
    $env:COINLINE_MMU_TRANSLATION_TRACE = (Join-Path $OutputDir 'mmu-translation-trace.jsonl')
}
if ($fullProfile -and $env:COINLINE_TRACE_INTERRUPTS -eq '1') {
    $env:COINLINE_INTERRUPT_TRACE = (Join-Path $OutputDir 'interrupt-trace.jsonl')
}
if ($fullProfile -and $env:COINLINE_TRACE_TIMERS -eq '1') {
    $env:COINLINE_TIMER_TRACE = (Join-Path $OutputDir 'timer-trace.jsonl')
}
if ($fullProfile -or $uartProfile -or $tpTimingProfile) {
    $env:COINLINE_ASCI_TRACE = (Join-Path $OutputDir 'asci-trace.jsonl')
    $env:COINLINE_EXTERNAL_UART_TRACE = (Join-Path $OutputDir 'external-uart-trace.jsonl')
    $env:COINLINE_TELEPHONY_BOARD_TRACE = (Join-Path $OutputDir 'telephony-board-trace.jsonl')
    $env:COINLINE_TELEPHONY_HANDSHAKE_TRACE = (Join-Path $OutputDir 'telephony-handshake-trace.jsonl')
    $env:COINLINE_TELEPHONY_PHASE_TRACE = (Join-Path $OutputDir 'telephony-phase-trace.jsonl')
    $env:COINLINE_TELEPHONY_READY_DECISION_TRACE = (Join-Path $OutputDir 'telephony-ready-decision-trace.jsonl')
    $env:COINLINE_TELEPHONY_RX_BUFFER_TRACE = (Join-Path $OutputDir 'telephony-rx-buffer-trace.jsonl')
    $env:COINLINE_TELEPHONY_PARSER_TRACE = (Join-Path $OutputDir 'telephony-parser-trace.jsonl')
}
if ($fullProfile -or $uartProfile -or $m6Profile -or $voiceProfile -or $tpTimingProfile) {
    $env:COINLINE_TRACE_PANEL = '1'
    $env:COINLINE_FRONT_PANEL_TRACE = (Join-Path $OutputDir 'front-panel-trace.jsonl')
    $env:COINLINE_FRONT_PANEL_INPUT_SOURCE_TRACE = (Join-Path $OutputDir 'front-panel-input-source-trace.jsonl')
}
if ($fullProfile) {
    $env:COINLINE_RESET_TRACE = (Join-Path $OutputDir 'reset-trace.jsonl')
}
if ($fullProfile -or $m6Profile) {
    $env:COINLINE_INTERRUPT_EVENTS = (Join-Path $OutputDir 'interrupt-events.jsonl')
    $env:COINLINE_VECTOR_EVENTS = (Join-Path $OutputDir 'vector-events.jsonl')
    $env:COINLINE_CONTEXT_SWITCH_EVENTS = (Join-Path $OutputDir 'context-switch-events.jsonl')
    $env:COINLINE_EIDI_EVENTS = (Join-Path $OutputDir 'ei-di-events.jsonl')
}
if ($fullProfile -or $m6Profile) {
    $env:COINLINE_FETCH_PROVENANCE_TRACE = (Join-Path $OutputDir 'fetch-provenance-trace.jsonl')
}
if ($fullProfile) {
    $env:COINLINE_STACK_CONTROL_FLOW_TRACE = (Join-Path $OutputDir 'stack-control-flow-trace.jsonl')
}
if ($VoicewareTrace -or $voiceProfile) {
    $env:COINLINE_TRACE_VOICEWARE = '1'
}
if ($AudioTrace -or $voiceProfile -or $fullProfile) {
    $env:COINLINE_TRACE_AUDIO = '1'
}
if ($env:COINLINE_TRACE_VOICEWARE -eq '1') {
    $env:COINLINE_VOICEWARE_TRACE = (Join-Path $OutputDir 'voiceware-trace.jsonl')
}
$env:COINLINE_VFD_TRACE = (Join-Path $OutputDir 'vfd-trace.jsonl')
$env:COINLINE_VFD_SNAPSHOTS = (Join-Path $OutputDir 'vfd-snapshots.jsonl')
$env:COINLINE_VOICEWARE_STARTUP_JSON = (Join-Path $OutputDir 'voiceware-startup-diagnostic.json')

$audioTracePath = Join-Path $OutputDir 'audio-trace.jsonl'
$routeTracePath = Join-Path $OutputDir 'audio-route-trace.jsonl'
$muteTracePath = Join-Path $OutputDir 'mute-route-trace.jsonl'
$telTracePath = Join-Path $OutputDir 'telephony-trace.jsonl'
$supTracePath = Join-Path $OutputDir 'supervision-trace.jsonl'
$altTracePath = Join-Path $OutputDir 'alerter-trace.jsonl'
if ($env:COINLINE_TRACE_AUDIO -eq '1' -or $AudioTrace) {
    $env:COINLINE_AUDIO_TRACE = $audioTracePath
}
if ($env:COINLINE_TRACE_AUDIO_ROUTE -eq '1' -or $env:COINLINE_TRACE_AUDIO -eq '1' -or $AudioTrace) {
    $env:COINLINE_AUDIO_ROUTE_TRACE = $routeTracePath
}
if ($env:COINLINE_TRACE_MUTE -eq '1' -or $env:COINLINE_TRACE_AUDIO -eq '1' -or $AudioTrace) {
    $env:COINLINE_MUTE_ROUTE_TRACE = $muteTracePath
}
if ($env:COINLINE_TRACE_TELEPHONY -eq '1' -or $env:COINLINE_TRACE_AUDIO -eq '1' -or $AudioTrace) {
    $env:COINLINE_TELEPHONY_TRACE = $telTracePath
}
if ($env:COINLINE_TRACE_SUPERVISION -eq '1') {
    $env:COINLINE_SUPERVISION_TRACE = $supTracePath
}
if ($env:COINLINE_TRACE_AUDIO -eq '1' -or $AudioTrace) {
    $env:COINLINE_ALERTER_TRACE = $altTracePath
}
$env:COINLINE_BOOT_TRACE = $bootTrace
$env:COINLINE_IO_TRACE = $ioTrace
$env:COINLINE_UNKNOWN_PORT_LOG = $unknownPort
if ($fullProfile) {
    $env:COINLINE_MEMORY_TRACE = $memTrace
    $env:COINLINE_CPU_TRACE = $cpuTrace
    $env:COINLINE_Z180_REG_TRACE = $z180Trace
} elseif ($m6Profile) {
    $env:COINLINE_CPU_TRACE = $cpuTrace
}
$env:COINLINE_MILESTONE_JSON = Join-Path $OutputDir 'milestones.json'

# Zero-byte touch (no UTF-8 BOM) so Windows append from MAME works; Set-Content UTF8 wrote BOM and blocked JSONL.
if (-not (Test-Path -LiteralPath $bootTrace)) { New-Item -ItemType File -Path $bootTrace -Force | Out-Null }
if (-not (Test-Path -LiteralPath $ioTrace)) { New-Item -ItemType File -Path $ioTrace -Force | Out-Null }
if ($env:COINLINE_NVRAM_STORAGE_TRACE -and ($env:COINLINE_NVRAM_STORAGE_TRACE -ne '')) {
    if (-not (Test-Path -LiteralPath $env:COINLINE_NVRAM_STORAGE_TRACE)) {
        New-Item -ItemType File -Path $env:COINLINE_NVRAM_STORAGE_TRACE -Force | Out-Null
    }
}
if ($env:COINLINE_MICROWIRE_TRACE -and ($env:COINLINE_MICROWIRE_TRACE -ne '')) {
    if (-not (Test-Path -LiteralPath $env:COINLINE_MICROWIRE_TRACE)) {
        New-Item -ItemType File -Path $env:COINLINE_MICROWIRE_TRACE -Force | Out-Null
    }
}
if ($fullProfile) {
    '' | Set-Content -LiteralPath $memTrace -Encoding UTF8
    '' | Set-Content -LiteralPath $cpuTrace -Encoding UTF8
    '' | Set-Content -LiteralPath $z180Trace -Encoding UTF8
} elseif ($m6Profile) {
    '' | Set-Content -LiteralPath $cpuTrace -Encoding UTF8
}
if ($fullProfile -or $m6Profile) {
    $v1 = Join-Path $OutputDir 'interrupt-events.jsonl'
    $v2 = Join-Path $OutputDir 'vector-events.jsonl'
    $v3 = Join-Path $OutputDir 'context-switch-events.jsonl'
    $v4 = Join-Path $OutputDir 'ei-di-events.jsonl'
    '' | Set-Content -LiteralPath $v1 -Encoding UTF8
    '' | Set-Content -LiteralPath $v2 -Encoding UTF8
    '' | Set-Content -LiteralPath $v3 -Encoding UTF8
    '' | Set-Content -LiteralPath $v4 -Encoding UTF8
}
if ($fullProfile -or $m6Profile) {
    '' | Set-Content -LiteralPath (Join-Path $OutputDir 'fetch-provenance-trace.jsonl') -Encoding UTF8
}
if ($fullProfile) {
    '' | Set-Content -LiteralPath (Join-Path $OutputDir 'stack-control-flow-trace.jsonl') -Encoding UTF8
}
if ($env:COINLINE_TRACE_VOICEWARE -eq '1') {
    '' | Set-Content -LiteralPath (Join-Path $OutputDir 'voiceware-trace.jsonl') -Encoding UTF8
}
if ($env:COINLINE_TRACE_AUDIO -eq '1' -or $AudioTrace) {
    foreach ($p in @($audioTracePath, $routeTracePath, $muteTracePath, $telTracePath, $supTracePath, $altTracePath)) {
        '' | Set-Content -LiteralPath $p -Encoding UTF8
    }
}
if ($env:COINLINE_TRACE_PANEL -eq '1') {
    if (-not $env:COINLINE_FRONT_PANEL_TRACE) {
        $env:COINLINE_FRONT_PANEL_TRACE = (Join-Path $OutputDir 'front-panel-trace.jsonl')
    }
    if (-not $env:COINLINE_FRONT_PANEL_INPUT_SOURCE_TRACE) {
        $env:COINLINE_FRONT_PANEL_INPUT_SOURCE_TRACE = (Join-Path $OutputDir 'front-panel-input-source-trace.jsonl')
    }
    if (-not $env:COINLINE_TP_CP_KEYPAD_PROTOCOL_TRACE) {
        $env:COINLINE_TP_CP_KEYPAD_PROTOCOL_TRACE = (Join-Path $OutputDir 'tp-cp-keypad-protocol-trace.jsonl')
    }
    if (-not $env:COINLINE_CRAFT_ENTRY_GATE_TRACE) {
        $env:COINLINE_CRAFT_ENTRY_GATE_TRACE = (Join-Path $OutputDir 'craft-entry-gate-trace.jsonl')
    }
    '' | Set-Content -LiteralPath $env:COINLINE_FRONT_PANEL_TRACE -Encoding UTF8
    $fpIn = $env:COINLINE_FRONT_PANEL_INPUT_SOURCE_TRACE
    '' | Set-Content -LiteralPath $fpIn -Encoding UTF8
}

$video = if ($Headless -or (-not $Windowed)) { 'none' } else { 'd3d' }
$verbose = @()
if ($MameVerbose) { $verbose = @('-verbose', '-log', '-oslog') }

$snapDir = Join-Path $OutputDir 'mame-snapshots'
New-Item -ItemType Directory -Force -Path $snapDir | Out-Null
$artDir = Join-Path $EmuRoot 'artwork'

$soundOpt = if ($EnableAudio) { 'auto' } else { 'none' }
$throttleOpt = if ($RealInputDemo) { '-throttle' } else { '-nothrottle' }
$cfgDir = Join-Path $OutputDir 'cfg'
New-Item -ItemType Directory -Force -Path $cfgDir | Out-Null
$nvramDir = if ($NvramDirectory -ne '') { $NvramDirectory } else { Join-Path $OutputDir 'nvram' }
New-Item -ItemType Directory -Force -Path $nvramDir | Out-Null
$nvramDirResolved = (Resolve-Path -LiteralPath $nvramDir).Path

if ($InitialNvramJson -ne '') {
    $ini = $InitialNvramJson
    if (-not [System.IO.Path]::IsPathRooted($ini)) {
        $ini = Join-Path $EmuRoot $ini
    }
    if (-not (Test-Path -LiteralPath $ini)) {
        throw "InitialNvramJson not found: $ini"
    }
    $env:COINLINE_NVRAM = $ini
}

$mameArgs = @(
    'cl_millennium',
    '-noreadconfig',
    '-cfg_directory', $cfgDir,
    '-nvram_directory', $nvramDirResolved,
    '-rompath', $runRomPath,
    '-artpath', $artDir,
    '-snapshot_directory', $snapDir,
    '-video', $video,
    '-sound', $soundOpt,
    $throttleOpt,
    '-seconds_to_run', "$RunSeconds"
) + $verbose
$fpInputTracePath = Join-Path $OutputDir 'front-panel-input-source-trace.jsonl'
$scriptedPanelDemo = ($env:COINLINE_SCRIPTED_PANEL_DEMO -eq '1')
$acceptanceModeRoot = ($env:COINLINE_ACCEPTANCE_MODE -eq '1')
$craftOnlyMode = ($env:COINLINE_CRAFT_ONLY_MODE -eq '1')
if ($RealInputDemo) {
    $mameArgs += @('-keyboardprovider', 'win32')
    if ($scriptedPanelDemo) {
        $realInputLua = Join-Path $OutputDir 'real-input-demo.lua'
        # Throttled ~60 Hz wall time: defer scripted pulses until the machine has run ~15–60 s of emulated time.
        $fHook = [int][Math]::Floor([Math]::Min(3600.0, [Math]::Max(900.0, $RunSeconds * 22.0)))
        $fOnHook = $fHook + 780
        $acceptanceMode = ($env:COINLINE_ACCEPTANCE_MODE -eq '1')
        if ($acceptanceMode) {
            # Run acceptance interaction later so firmware has time to settle into OOS/service state.
            $fHook = [int][Math]::Min([Math]::Max(2400, $fHook), 3000)
            if ($craftOnlyMode) {
                # A6-focused loop: stay on-hook, but defer keypad sequence until OOS/TP-ready has time to settle.
                $fHook = [int][Math]::Min([Math]::Max(3000, $fHook), 3400)
            }
            # Short off-hook pulse for acceptance A2/A4 path; craft-only keeps line on-hook.
            $fOnHook = if ($craftOnlyMode) { $fHook } else { $fHook + 180 }
            # Craft/install: cabinet cfg may already assert lock/door/vault; do not pulse toggles (risks wrong state).
            # Service is non-toggle — hold during the install digit window.
            $fSecGateStart = $fOnHook + 30
            $dig = 90
            $fK2a = $fOnHook + 120
            $fK2ac = $fK2a + $dig
            $fK7a = $fK2ac + $dig
            $fK7ac = $fK7a + $dig
            $fK2b = $fK7ac + $dig
            $fK2bc = $fK2b + $dig
            $fK7b = $fK2bc + $dig
            $fK7bc = $fK7b + $dig
            $fK3 = $fK7bc + $dig
            $fK3c = $fK3 + $dig
            $fK7c = $fK3c + $dig
            $fK7cc = $fK7c + $dig
            $fK8 = $fK7cc + $dig
            $fK8c = $fK8 + $dig
            $fSecGateEnd = $fK8c + 180
            $fEnd = $fK8c + 300
        } else {
            $fK1 = $fHook + 120
            $fK1c = $fHook + 180
            $fK2 = $fHook + 240
            $fK2c = $fHook + 300
            $fK3 = $fHook + 360
            $fK3c = $fHook + 420
            $fKp = $fHook + 480
            $fKpc = $fHook + 540
            $fEnd = $fHook + 780
        }
        $realInputLuaBody = @"
local ports = manager.machine.ioport.ports
local km = ports[":KEYMATRIX"] or ports["KEYMATRIX"]
local line = ports[":LINECTRL"] or ports["LINECTRL"]
local sec = ports[":SECMASK"] or ports["SECMASK"]
if km == nil then return end

local function field(port, mask)
  if port == nil then return nil end
  return port:field(mask)
end

local handset = field(km, 0x00080000)
local k1 = field(km, 0x00000001)
local k2 = field(km, 0x00000002)
local k3 = field(km, 0x00000004)
local k7 = field(km, 0x00000040)
local k8 = field(km, 0x00000080)
local kp = field(km, 0x00000800)
local hook = field(line, 0x01)
local svc = field(sec, 0x08)

local frame = 0
emu.register_frame_done(function()
  frame = frame + 1
$(if ($acceptanceMode) {
@"
"@
} else {
@"
  if frame >= $fHook and frame < $fOnHook and hook ~= nil then hook:set_value(1) end
  if frame >= $fOnHook and frame < $fEnd and hook ~= nil then hook:clear_value() end
  if frame >= $fEnd and hook ~= nil then hook:clear_value() end
"@
})
$(if ($acceptanceMode) {
@"
$(if (-not $craftOnlyMode) {
@"
  if frame == $fHook and handset ~= nil then handset:set_value(1) end
  if frame == $($fHook + 1) and handset ~= nil then handset:clear_value() end
"@
} else {
@"
"@
})
$(if (-not $craftOnlyMode) {
@"
  if frame == $fOnHook and handset ~= nil then handset:set_value(1) end
  if frame == $($fOnHook + 1) and handset ~= nil then handset:clear_value() end
"@
} else {
@"
"@
})
  if frame >= $fSecGateStart and frame < $fSecGateEnd then
    if svc ~= nil then svc:set_value(1) end
  else
    if svc ~= nil then svc:clear_value() end
  end
  if frame == $fK2a and k2 ~= nil then k2:set_value(1) end
  if frame == $fK2ac and k2 ~= nil then k2:clear_value() end
  if frame == $fK7a and k7 ~= nil then k7:set_value(1) end
  if frame == $fK7ac and k7 ~= nil then k7:clear_value() end
  if frame == $fK2b and k2 ~= nil then k2:set_value(1) end
  if frame == $fK2bc and k2 ~= nil then k2:clear_value() end
  if frame == $fK7b and k7 ~= nil then k7:set_value(1) end
  if frame == $fK7bc and k7 ~= nil then k7:clear_value() end
  if frame == $fK3 and k3 ~= nil then k3:set_value(1) end
  if frame == $fK3c and k3 ~= nil then k3:clear_value() end
  if frame == $fK7c and k7 ~= nil then k7:set_value(1) end
  if frame == $fK7cc and k7 ~= nil then k7:clear_value() end
  if frame == $fK8 and k8 ~= nil then k8:set_value(1) end
  if frame == $fK8c and k8 ~= nil then k8:clear_value() end
"@
} else {
@"
  if frame >= $fHook and frame < $fOnHook and handset ~= nil then handset:set_value(1) end
  if frame >= $fOnHook and frame < $fEnd and handset ~= nil then handset:clear_value() end
  if frame >= $fEnd and handset ~= nil then handset:clear_value() end
  if frame == $fK1 and k1 ~= nil then k1:set_value(1) end
  if frame == $fK1c and k1 ~= nil then k1:clear_value() end
  if frame == $fK2 and k2 ~= nil then k2:set_value(1) end
  if frame == $fK2c and k2 ~= nil then k2:clear_value() end
  if frame == $fK3 and k3 ~= nil then k3:set_value(1) end
  if frame == $fK3c and k3 ~= nil then k3:clear_value() end
  if frame == $fKp and kp ~= nil then kp:set_value(1) end
  if frame == $fKpc and kp ~= nil then kp:clear_value() end
  if frame == $fEnd and handset ~= nil then handset:clear_value() end
  if frame == $fEnd and hook ~= nil then hook:clear_value() end
"@
})
end, "real_input_demo")
"@
        [System.IO.File]::WriteAllText($realInputLua, $realInputLuaBody, (New-Object System.Text.UTF8Encoding($false)))
        $mameArgs += @('-autoboot_delay', '1', '-autoboot_script', $realInputLua)
        if ($env:COINLINE_TRACE_PANEL -eq '1') {
            $wallMs = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
            $pre = "{`"event`":`"input_demo_started`",`"wall_time_ms`":$wallMs,`"input_source`":`"scripted_mame_input`",`"note`":`"lua_autoboot_registered`"}"
            Add-Content -LiteralPath $fpInputTracePath -Value $pre -Encoding UTF8
            $hookInputName = if ($acceptanceMode) { 'KEYMATRIX' } else { 'LINECTRL+KEYMATRIX' }
            $hookEventName = if ($craftOnlyMode) { 'input_hook_offhook_skipped_craft_only' } else { 'input_hook_offhook_sent' }
            $hookNote = if ($craftOnlyMode) { 'craft_only_mode_keeps_onhook' } else { "lua_frame_$fHook" }
            $sched = "{`"event`":`"$hookEventName`",`"wall_time_ms`":$wallMs,`"input_source`":`"scripted_mame_input`",`"note`":`"$hookNote`",`"MAME_input_name`":`"$hookInputName`"}"
            Add-Content -LiteralPath $fpInputTracePath -Value $sched -Encoding UTF8
            if ($acceptanceMode) {
                if (-not $craftOnlyMode) {
                    Add-Content -LiteralPath $fpInputTracePath -Value "{`"event`":`"input_hook_onhook_sent`",`"wall_time_ms`":$wallMs,`"input_source`":`"scripted_mame_input`",`"note`":`"lua_frame_$fOnHook`",`"MAME_input_name`":`"KEYMATRIX`"}" -Encoding UTF8
                } else {
                    Add-Content -LiteralPath $fpInputTracePath -Value "{`"event`":`"input_hook_onhook_preserved`",`"wall_time_ms`":$wallMs,`"input_source`":`"scripted_mame_input`",`"note`":`"craft_only_mode_no_offhook_toggle`",`"MAME_input_name`":`"KEYMATRIX`"}" -Encoding UTF8
                }
                Add-Content -LiteralPath $fpInputTracePath -Value "{`"event`":`"input_sec_door_service_gate`",`"wall_time_ms`":$wallMs,`"input_source`":`"scripted_mame_input`",`"note`":`"lua_frames_$($fSecGateStart)_to_$($fSecGateEnd)`",`"MAME_input_name`":`"SECMASK`"}" -Encoding UTF8
                Add-Content -LiteralPath $fpInputTracePath -Value "{`"event`":`"input_key_digit_2_sent`",`"wall_time_ms`":$wallMs,`"input_source`":`"scripted_mame_input`",`"note`":`"lua_frame_$fK2a`"}" -Encoding UTF8
                Add-Content -LiteralPath $fpInputTracePath -Value "{`"event`":`"input_key_digit_7_sent`",`"wall_time_ms`":$wallMs,`"input_source`":`"scripted_mame_input`",`"note`":`"lua_frame_$fK7a`"}" -Encoding UTF8
                Add-Content -LiteralPath $fpInputTracePath -Value "{`"event`":`"input_key_digit_2_sent`",`"wall_time_ms`":$wallMs,`"input_source`":`"scripted_mame_input`",`"note`":`"lua_frame_$fK2b`"}" -Encoding UTF8
                Add-Content -LiteralPath $fpInputTracePath -Value "{`"event`":`"input_key_digit_7_sent`",`"wall_time_ms`":$wallMs,`"input_source`":`"scripted_mame_input`",`"note`":`"lua_frame_$fK7b`"}" -Encoding UTF8
                Add-Content -LiteralPath $fpInputTracePath -Value "{`"event`":`"input_key_digit_3_sent`",`"wall_time_ms`":$wallMs,`"input_source`":`"scripted_mame_input`",`"note`":`"lua_frame_$fK3`"}" -Encoding UTF8
                Add-Content -LiteralPath $fpInputTracePath -Value "{`"event`":`"input_key_digit_7_sent`",`"wall_time_ms`":$wallMs,`"input_source`":`"scripted_mame_input`",`"note`":`"lua_frame_$fK7c`"}" -Encoding UTF8
                Add-Content -LiteralPath $fpInputTracePath -Value "{`"event`":`"input_key_digit_8_sent`",`"wall_time_ms`":$wallMs,`"input_source`":`"scripted_mame_input`",`"note`":`"lua_frame_$fK8`"}" -Encoding UTF8
            }
        }
    }
}
$wavOutPath = Join-Path $OutputDir 'voiceware-output.wav'
if ($WavWrite) {
    $mameArgs += @('-wavwrite', $wavOutPath)
}

$runLog = Join-Path $OutputDir 'run.log'
$stdoutPath = Join-Path $OutputDir 'mame-stdout.log'
$stderrPath = Join-Path $OutputDir 'mame-stderr.log'

@"
run-screenshot-capture.ps1
mame_exe=$MameExe
args=$($mameArgs -join ' ')
firmware=$FirmwareBinary
run_seconds=$RunSeconds
headless=$Headless
windowed=$Windowed
microwire_early_stop_idle_seconds=$MicrowireEarlyStopIdleSeconds
microwire_early_stop_min_jsonl_lines=$MicrowireEarlyStopMinJsonlLines
microwire_early_stop_warm_seconds=$MicrowireEarlyStopWarmSeconds
microwire_early_stop_require_committed_write=$MicrowireEarlyStopRequireCommittedWrite
"@ | Set-Content -LiteralPath $runLog -Encoding UTF8

$mameCommit = ''
$mameDir = Join-Path $EmuRoot 'third-party\mame'
if (Test-Path (Join-Path $mameDir '.git')) {
    try {
        $mameCommit = (& git -C $mameDir rev-parse HEAD 2>$null).Trim()
    }
    catch {}
}

function Invoke-MameScreenshot {
    param([string]$DestPath)
    $capScript = Join-Path $PSScriptRoot 'capture-mame-screenshot.ps1'
    if (-not (Test-Path -LiteralPath $capScript)) { return $false }
    try {
        & $capScript -RunDir $OutputDir -OutputPath $DestPath
        return (Test-Path -LiteralPath $DestPath)
    }
    catch {
        return $false
    }
}

function Invoke-RealInputDemo {
    param(
        [System.Diagnostics.Process]$Process,
        [string]$PanelInputTracePath = ''
    )
    try {
        Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class WinApiFocus {
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern IntPtr SetFocus(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern void keybd_event(byte bVk, byte bScan, uint dwFlags, UIntPtr dwExtraInfo);
}
"@ -ErrorAction SilentlyContinue
        $wshell = New-Object -ComObject WScript.Shell
        $deadline = (Get-Date).AddSeconds(20)
        $windowReady = $false
        while ((Get-Date) -lt $deadline) {
            if ($Process.HasExited) { return $false }
            $Process.Refresh()
            if ($Process.MainWindowHandle -ne 0) {
                [void][WinApiFocus]::ShowWindow($Process.MainWindowHandle, 9)
                [void][WinApiFocus]::SetForegroundWindow($Process.MainWindowHandle)
                [void][WinApiFocus]::SetFocus($Process.MainWindowHandle)
                $windowReady = $true
                if ($PanelInputTracePath -and (Test-Path -LiteralPath $PanelInputTracePath)) {
                    $wrMs = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
                    $wrLn = "{`"event`":`"input_window_ready`",`"wall_time_ms`":$wrMs,`"input_source`":`"keyboard`",`"note`":`"mame_window_focused_for_sendinput`"}"
                    Add-Content -LiteralPath $PanelInputTracePath -Value $wrLn -Encoding UTF8
                }
                break
            }
            Start-Sleep -Milliseconds 200
        }
        if (-not $windowReady) { return $false }
        Start-Sleep -Seconds 16
        function Get-SendKeysToken([int]$vk) {
            switch ($vk) {
                0x55 { return 'u' }
                0x48 { return 'h' }
                0x30 { return '0' }
                0x31 { return '1' }
                0x32 { return '2' }
                0x33 { return '3' }
                0x60 { return '{NUMPAD0}' }
                0x61 { return '{NUMPAD1}' }
                0x62 { return '{NUMPAD2}' }
                0x63 { return '{NUMPAD3}' }
                0x6F { return '{DIVIDE}' }
                default { return $null }
            }
        }
        $acceptanceMode = ($env:COINLINE_ACCEPTANCE_MODE -eq '1')
        if ($acceptanceMode) {
            # NIS_CRAFT_INSTALL_ACCEPTANCE sequence:
            # handset off-hook -> wait 10s -> handset on-hook -> wait 2s -> keypad 2727378 via real keyboard events.
            $steps = @(
                @{ vk = 0x48; hold = 300; keepDown = $false; note = 'handset_toggle_offhook' }, # H toggle on
                @{ waitOnly = $true; hold = 10000; note = 'offhook_wait_10s' },
                @{ vk = 0x48; hold = 300; keepDown = $false; note = 'handset_toggle_onhook' }, # H toggle off
                @{ waitOnly = $true; hold = 2000; note = 'onhook_wait_2s' },
                @{ vk = 0x32; hold = 420; keepDown = $false; note = 'digit_2' },
                @{ vk = 0x37; hold = 420; keepDown = $false; note = 'digit_7' },
                @{ vk = 0x32; hold = 420; keepDown = $false; note = 'digit_2' },
                @{ vk = 0x37; hold = 420; keepDown = $false; note = 'digit_7' },
                @{ vk = 0x33; hold = 420; keepDown = $false; note = 'digit_3' },
                @{ vk = 0x37; hold = 420; keepDown = $false; note = 'digit_7' },
                @{ vk = 0x38; hold = 420; keepDown = $false; note = 'digit_8' }
            )
        }
        else {
            $steps = @(
                # Toggle hook supervision first using U (LINECTRL Hook Toggle), then hold/release H (handset keymatrix).
                @{ vk = 0x55; hold = 700; keepDown = $false },  # U
                @{ vk = 0x48; hold = 1800; keepDown = $true },  # H down
                # Send both top-row and numpad digits so MAME bindings on either code path are exercised.
                @{ vk = 0x31; hold = 700; keepDown = $false },  # 1
                @{ vk = 0x61; hold = 700; keepDown = $false },  # numpad 1
                @{ vk = 0x32; hold = 700; keepDown = $false },  # 2
                @{ vk = 0x62; hold = 700; keepDown = $false },  # numpad 2
                @{ vk = 0x33; hold = 700; keepDown = $false },  # 3
                @{ vk = 0x63; hold = 700; keepDown = $false },  # numpad 3
                @{ vk = 0x30; hold = 700; keepDown = $false },  # 0
                @{ vk = 0x60; hold = 700; keepDown = $false },  # numpad 0
                @{ vk = 0x6F; hold = 900; keepDown = $false },  # numpad divide => keypad #
                @{ vk = 0x48; hold = 250; releaseOnly = $true }, # H up
                @{ vk = 0x55; hold = 500; keepDown = $false }   # U toggle back
            )
        }
        foreach ($s in $steps) {
            if ($Process.HasExited) { return $false }
            if ($s.waitOnly) {
                if ($PanelInputTracePath -and (Test-Path -LiteralPath $PanelInputTracePath)) {
                    $tm = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
                    $ln = "{`"event`":`"input_step_wait`",`"wall_time_ms`":$tm,`"input_source`":`"keyboard`",`"wait_ms`":$($s.hold),`"note`":`"$($s.note)`"}"
                    Add-Content -LiteralPath $PanelInputTracePath -Value $ln -Encoding UTF8
                }
                Start-Sleep -Milliseconds $s.hold
                continue
            }
            $Process.Refresh()
            if ($Process.MainWindowHandle -ne 0) {
                [void][WinApiFocus]::ShowWindow($Process.MainWindowHandle, 9) # SW_RESTORE
                [void][WinApiFocus]::SetForegroundWindow($Process.MainWindowHandle)
                [void][WinApiFocus]::SetFocus($Process.MainWindowHandle)
            }
            [void]$wshell.AppActivate($Process.Id)
            Start-Sleep -Milliseconds 100
            if (-not $s.releaseOnly) {
                [WinApiFocus]::keybd_event([byte]$s.vk, [byte]0, [uint32]0, [UIntPtr]::Zero)
                $tok = Get-SendKeysToken -vk ([int]$s.vk)
                if ($tok) {
                    [void]$wshell.SendKeys($tok)
                }
                if ($PanelInputTracePath -and (Test-Path -LiteralPath $PanelInputTracePath)) {
                    $tm = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
                    $note = if ($s.note) { $s.note } else { "vk_0x{0:X2}" -f ([int]$s.vk) }
                    $ln = "{`"event`":`"input_key_sent`",`"wall_time_ms`":$tm,`"input_source`":`"keyboard`",`"vk`":`"0x$('{0:X2}' -f ([int]$s.vk))`",`"note`":`"$note`"}"
                    Add-Content -LiteralPath $PanelInputTracePath -Value $ln -Encoding UTF8
                }
            }
            Start-Sleep -Milliseconds $s.hold
            if (-not $s.keepDown) {
                [WinApiFocus]::keybd_event([byte]$s.vk, [byte]0, [uint32]2, [UIntPtr]::Zero)
                Start-Sleep -Milliseconds 220
            }
        }
        return $true
    }
    catch {
        return $false
    }
}

function Keep-MameWindowVisible {
    param([System.Diagnostics.Process]$Process)
    try {
        if ($Process -eq $null -or $Process.HasExited) { return }
        $Process.Refresh()
        if ($Process.MainWindowHandle -eq 0) { return }
        [void][WinApiFocus]::ShowWindow($Process.MainWindowHandle, 9) # SW_RESTORE
        [void][WinApiFocus]::SetForegroundWindow($Process.MainWindowHandle)
    }
    catch {
    }
}

function Write-VfdRenderedPng {
    param(
        [string]$Path,
        [string]$Heading,
        [string]$BodyLines,
        [string]$FooterLabel
    )
    Add-Type -AssemblyName System.Drawing
    $w = 880
    $h = 420
    $bmp = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.Clear([System.Drawing.Color]::FromArgb(255, 12, 12, 12))
    $titleFont = New-Object System.Drawing.Font ('Segoe UI'), ([float]11), ([System.Drawing.FontStyle]::Bold)
    $bodyFont = New-Object System.Drawing.Font ('Consolas'), ([float]13), ([System.Drawing.FontStyle]::Regular)
    $small = New-Object System.Drawing.Font ('Segoe UI'), ([float]9), ([System.Drawing.FontStyle]::Regular)
    $brushHi = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::LightGray)
    $brushBody = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 180, 255, 180))
    $brushFoot = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::Orange)
    $g.DrawString($Heading, $titleFont, $brushHi, 16, 12)
    $g.DrawString($BodyLines, $bodyFont, $brushBody, 16, 44)
    $g.DrawString($FooterLabel, $small, $brushFoot, 16, ($h - 52))
    $bmp.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose()
    $bmp.Dispose()
}

function Test-CoinlineNis2727378Sequence {
    param(
        [string]$PanelPath,
        [string]$TbPath
    )
    $want = @('2', '7', '2', '7', '3', '7', '8')
    if (Test-Path -LiteralPath $PanelPath) {
        $wi = 0
        Get-Content -LiteralPath $PanelPath | ForEach-Object {
            if ($_ -match 'input_key_digit_(\d)_sent') {
                if ($matches[1] -eq $want[$wi]) { $wi++ }
            }
        }
        if ($wi -eq $want.Length) { return $true }
    }
    if (-not (Test-Path -LiteralPath $TbPath)) { return $false }
    $got = New-Object System.Collections.Generic.List[string]
    Get-Content -LiteralPath $TbPath | ForEach-Object {
        if ($_ -notmatch 'telephony_tp_ui_event') { return }
        if ($_ -notmatch '"tp_to_cp":"(0x[0-9A-Fa-f]{2})"') { return }
        $b = [Convert]::ToInt32($matches[1], 16)
        $map = @{ 0x24 = '2'; 0x2E = '7'; 0x26 = '3'; 0x30 = '8' }
        if ($map.ContainsKey($b)) { [void]$got.Add($map[$b]) }
    }
    $joined = -join $got
    return $joined.Contains('2727378')
}

$microwireEarlyStopUsed = $false
$runStart = Get-Date
if ($Headless -or -not $Windowed) {
    $p = Start-Process -FilePath $MameExe -ArgumentList $mameArgs -WorkingDirectory $EmuRoot `
        -PassThru -WindowStyle Normal `
        -RedirectStandardOutput $stdoutPath `
        -RedirectStandardError $stderrPath
}
else {
    # Redirecting stdio can prevent a normal top-level window on some builds; omit for GUI evidence.
    $p = Start-Process -FilePath $MameExe -ArgumentList $mameArgs -WorkingDirectory $EmuRoot `
        -PassThru -WindowStyle Normal
}

$realInputDemoRan = $false
$realInputDemoPsOk = $false
if ($RealInputDemo -and -not $Headless -and $Windowed -and -not $scriptedPanelDemo) {
    if (-not $scriptedPanelDemo) {
        $realInputDemoPsOk = Invoke-RealInputDemo -Process $p -PanelInputTracePath $fpInputTracePath
    }
}

$captureBootOk = $false
$captureAd1Ok = $false
$captureAd2Ok = $false
$captureFinalOk = $false
try {
    if ($guiUsed) {
        $WaitWhileRunning = {
            param([int]$Seconds)
            $deadline = (Get-Date).AddSeconds($Seconds)
            while ((Get-Date) -lt $deadline) {
                if ($p.HasExited) { return $false }
                if ($RealInputDemo) { Keep-MameWindowVisible -Process $p }
                Start-Sleep -Milliseconds 250
            }
            return (-not $p.HasExited)
        }

        $mwTraceRaw = $env:COINLINE_MICROWIRE_TRACE
        $mwTraceEarly = ($MicrowireEarlyStopIdleSeconds -gt 0) -and $mwTraceRaw -and ($mwTraceRaw.Trim() -ne '')

        if ($mwTraceEarly) {
            $microwireEarlyStopUsed = $true
            $mwPath = $mwTraceRaw
            $warmEarly = [Math]::Max(3, $MicrowireEarlyStopWarmSeconds)
            if (& $WaitWhileRunning $warmEarly) {
                $captureBootOk = Invoke-MameScreenshot -DestPath (Join-Path $OutputDir 'boot-normal-start.png')
                if ($captureBootOk) {
                    Copy-Item -LiteralPath (Join-Path $OutputDir 'boot-normal-start.png') -Destination (Join-Path $OutputDir 'final-screen.png') -Force
                    $captureFinalOk = $true
                }
            }

            $pollMs = [Math]::Max(200, $MicrowireEarlyStopPollMilliseconds)
            $deadlineIdle = (Get-Date).AddSeconds([Math]::Max(0, $RunSeconds - $warmEarly))
            $lastSize = [int64]-1
            $idleSince = $null
            $stoppedForIdle = $false
            while ((Get-Date) -lt $deadlineIdle -and -not $p.HasExited) {
                if ($RealInputDemo) { Keep-MameWindowVisible -Process $p }
                Start-Sleep -Milliseconds $pollMs
                $size = [int64]0
                $lineCount = 0
                $hasMwWrite = $false
                if (Test-Path -LiteralPath $mwPath) {
                    $fi = Get-Item -LiteralPath $mwPath
                    $size = $fi.Length
                    $lineCount = @(Get-Content -LiteralPath $mwPath -ErrorAction SilentlyContinue | Where-Object { $_.Trim().Length -gt 0 }).Count
                    $hasMwWrite = (@(Select-String -LiteralPath $mwPath -Pattern '"op":"write"' -ErrorAction SilentlyContinue)).Count -gt 0
                }
                if ($lineCount -lt [Math]::Max(1, $MicrowireEarlyStopMinJsonlLines)) {
                    $idleSince = $null
                    $lastSize = $size
                    continue
                }
                if ($size -eq $lastSize) {
                    if ($null -eq $idleSince) { $idleSince = Get-Date }
                    elseif (((Get-Date) - $idleSince).TotalSeconds -ge $MicrowireEarlyStopIdleSeconds) {
                        if ($MicrowireEarlyStopRequireCommittedWrite -and -not $hasMwWrite) {
                            $idleSince = $null
                        }
                        else {
                            $stoppedForIdle = $true
                            break
                        }
                    }
                }
                else {
                    $idleSince = $null
                    $lastSize = $size
                }
            }

            $earlyNote = "microwire_early_stop idle_sec=$MicrowireEarlyStopIdleSeconds min_lines=$MicrowireEarlyStopMinJsonlLines warm_sec=$warmEarly require_write=$MicrowireEarlyStopRequireCommittedWrite stopped_for_idle=$stoppedForIdle mw_path=$mwPath"
            Add-Content -LiteralPath $runLog -Value $earlyNote -Encoding UTF8

            # Short post-burst UI settle + same screenshot filenames as the long run.
            if (-not $p.HasExited) {
                Start-Sleep -Seconds 1
                if (& $WaitWhileRunning 1) {
                    $captureAd1Ok = Invoke-MameScreenshot -DestPath (Join-Path $OutputDir 'post-install-ad-scroll.png')
                    if ($captureAd1Ok) {
                        Copy-Item -LiteralPath (Join-Path $OutputDir 'post-install-ad-scroll.png') -Destination (Join-Path $OutputDir 'final-screen.png') -Force
                        $captureFinalOk = $true
                    }
                }
                if (-not $p.HasExited) {
                    Start-Sleep -Seconds 1
                    [void](& $WaitWhileRunning 1)
                    if (-not $p.HasExited) {
                        $captureAd2Ok = Invoke-MameScreenshot -DestPath (Join-Path $OutputDir 'post-install-ad-scroll-2.png')
                        if ($captureAd2Ok) {
                            Copy-Item -LiteralPath (Join-Path $OutputDir 'post-install-ad-scroll-2.png') -Destination (Join-Path $OutputDir 'final-screen.png') -Force
                            $captureFinalOk = $true
                        }
                    }
                }
                if (-not $p.HasExited) {
                    $captureFinalOk = Invoke-MameScreenshot -DestPath (Join-Path $OutputDir 'final-screen.png')
                }
            }
        }
        else {
            $warm = [Math]::Min([Math]::Max(3, [int]($RunSeconds / 3)), [Math]::Max(3, $RunSeconds - 2))
            if (& $WaitWhileRunning $warm) {
                $captureBootOk = Invoke-MameScreenshot -DestPath (Join-Path $OutputDir 'boot-normal-start.png')
                if ($captureBootOk) {
                    Copy-Item -LiteralPath (Join-Path $OutputDir 'boot-normal-start.png') -Destination (Join-Path $OutputDir 'final-screen.png') -Force
                    $captureFinalOk = $true
                }
            }

            $midWait = [Math]::Max(0, [int]($RunSeconds * 0.6) - $warm)
            if ($midWait -gt 0 -and (& $WaitWhileRunning $midWait)) {
                $captureAd1Ok = Invoke-MameScreenshot -DestPath (Join-Path $OutputDir 'post-install-ad-scroll.png')
                if ($captureAd1Ok) {
                    Copy-Item -LiteralPath (Join-Path $OutputDir 'post-install-ad-scroll.png') -Destination (Join-Path $OutputDir 'final-screen.png') -Force
                    $captureFinalOk = $true
                }
            }

            $tailWait = [Math]::Max(0, $RunSeconds - $warm - $midWait - 2)
            if ($tailWait -gt 0 -and (& $WaitWhileRunning $tailWait)) {
                $captureAd2Ok = Invoke-MameScreenshot -DestPath (Join-Path $OutputDir 'post-install-ad-scroll-2.png')
                if ($captureAd2Ok) {
                    Copy-Item -LiteralPath (Join-Path $OutputDir 'post-install-ad-scroll-2.png') -Destination (Join-Path $OutputDir 'final-screen.png') -Force
                    $captureFinalOk = $true
                }
            }
            $elapsed = [int]((Get-Date) - $runStart).TotalSeconds
            $toFinal = [Math]::Max(0, ($RunSeconds - 1) - $elapsed)
            if ($toFinal -gt 0) {
                [void](& $WaitWhileRunning $toFinal)
            }
            if (-not $p.HasExited) {
                $captureFinalOk = Invoke-MameScreenshot -DestPath (Join-Path $OutputDir 'final-screen.png')
            }
        }
    }
}
catch {
    Set-Content -LiteralPath (Join-Path $OutputDir 'screenshot-capture-failed.md') -Value "Screenshot capture exception: $_" -Encoding UTF8
}

$gracefulStop = $false
$forcedStop = $false

# Let MAME complete its own -seconds_to_run shutdown so wavwrite can finalize RIFF/data sizes.
if (-not $microwireEarlyStopUsed) {
    Wait-Process -Id $p.Id -Timeout 20 -ErrorAction SilentlyContinue
}
else {
    Add-Content -LiteralPath $runLog -Value 'microwire_early_stop: skipping Wait-Process for natural -seconds_to_run exit' -Encoding UTF8
}
if (-not $p.HasExited) {
    try {
        if ($p.MainWindowHandle -ne 0) {
            $gracefulStop = $p.CloseMainWindow()
            if ($gracefulStop) {
                Add-Content -LiteralPath $runLog -Value "requested_graceful_close_main_window=true" -Encoding UTF8
                Wait-Process -Id $p.Id -Timeout 10 -ErrorAction SilentlyContinue
            }
        }
    }
    catch {
        Add-Content -LiteralPath $runLog -Value "graceful_close_exception=$($_.Exception.Message)" -Encoding UTF8
    }
}
if (-not $p.HasExited) {
    Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
    $forcedStop = $true
    $fsReason = if ($microwireEarlyStopUsed) { 'forced_stop_after_microwire_early_stop_or_close' } else { "forced_stop_after_wall_clock_seconds=$RunSeconds" }
    Add-Content -LiteralPath $runLog -Value $fsReason -Encoding UTF8
}

if (Test-Path -LiteralPath $stdoutPath) {
    Get-Content -LiteralPath $stdoutPath -ErrorAction SilentlyContinue | Add-Content -LiteralPath $runLog -Encoding UTF8
}
else {
    Add-Content -LiteralPath $runLog -Value '(windowed run: stdout not redirected to file)' -Encoding UTF8
}
Add-Content -LiteralPath $runLog -Value '--- stderr ---' -Encoding UTF8
if (Test-Path -LiteralPath $stderrPath) {
    Get-Content -LiteralPath $stderrPath -ErrorAction SilentlyContinue | Add-Content -LiteralPath $runLog -Encoding UTF8
}

if ($RealInputDemo -and (Test-Path -LiteralPath $fpInputTracePath)) {
    $wallDone = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
    $doneLine = "{`"event`":`"input_demo_completed`",`"wall_time_ms`":$wallDone,`"input_source`":`"script_launcher`",`"note`":`"post_mame_exit`"}"
    Add-Content -LiteralPath $fpInputTracePath -Value $doneLine -Encoding UTF8
}
$traceHookMame = $false
$traceKeyMame = $false
$traceHookFw = $false
$traceKeyFw = $false
$telephonyBoardTracePath = Join-Path $OutputDir 'telephony-board-trace.jsonl'
$traceNis2727378 = Test-CoinlineNis2727378Sequence -PanelPath $fpInputTracePath -TbPath $telephonyBoardTracePath
if (Test-Path -LiteralPath $fpInputTracePath) {
    Get-Content -LiteralPath $fpInputTracePath -ErrorAction SilentlyContinue | ForEach-Object {
        $ln = $_.Trim()
        if (-not $ln) { return }
        if ($ln -match 'input_hook_offhook_seen_by_mame') { $traceHookMame = $true }
        if ($ln -match 'input_key_1_seen_by_mame|input_key_pound_seen_by_mame') { $traceKeyMame = $true }
        if ($ln -match 'input_hook_offhook_read_by_firmware') { $traceHookFw = $true }
        if ($ln -match 'input_key_1_read_by_firmware|input_key_pound_read_by_firmware') { $traceKeyFw = $true }
    }
}
if ($RealInputDemo) {
    if ($scriptedPanelDemo -and $acceptanceModeRoot) {
        $traceBacked = [bool]($traceHookMame -and $traceNis2727378)
    }
    elseif ($scriptedPanelDemo) {
        $traceBacked = ($traceHookMame -and $traceKeyMame) -or ($traceHookFw -and $traceKeyFw)
    }
    else {
        $traceBacked = ($traceHookMame -and $traceKeyMame) -or ($traceHookFw -and $traceKeyFw)
    }
    if ($scriptedPanelDemo) {
        $realInputDemoRan = [bool]$traceBacked
    }
    else {
        $realInputDemoRan = [bool]($traceBacked -or $realInputDemoPsOk)
    }
}

$lastKeymatrix = ''
$lastLinectrl = ''
if (Test-Path -LiteralPath $fpInputTracePath) {
    Get-Content -LiteralPath $fpInputTracePath -ErrorAction SilentlyContinue | ForEach-Object {
        $ln = $_.Trim()
        if (-not $ln) { return }
        try {
            $jo = $ln | ConvertFrom-Json
            $nm = [string]$jo.MAME_input_name
            if ($nm -eq 'KEYMATRIX' -and $jo.new_input_state) { $lastKeymatrix = [string]$jo.new_input_state }
            if ($nm -eq 'LINECTRL' -and $jo.new_input_state) { $lastLinectrl = [string]$jo.new_input_state }
        }
        catch {}
    }
}
[ordered]@{
    schema_version          = 'coinline.input_state_final/v1'
    keymatrix_input_hex     = $lastKeymatrix
    linectrl_input_hex      = $lastLinectrl
    hook_seen_mame          = [bool]$traceHookMame
    keypad_seen_mame        = [bool]$traceKeyMame
    hook_read_by_firmware   = [bool]$traceHookFw
    keypad_read_by_firmware = [bool]$traceKeyFw
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $OutputDir 'input-state-final.json') -Encoding UTF8

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

$bootMilestonesPath = Join-Path $OutputDir 'boot-milestones.json'
Convert-JsonlFileToJsonArray -Path $bootTrace -OutJson $bootMilestonesPath

$vfdTraceOut = Join-Path $OutputDir 'vfd-trace.jsonl'
if ((Test-Path -LiteralPath $vfdTraceOut) -and ((Get-Item -LiteralPath $vfdTraceOut).Length -gt 2)) {
    # Emulator-produced VFD trace is authoritative when present.
}
elseif (Test-Path -LiteralPath $ioTrace) {
    Copy-Item -LiteralPath $ioTrace -Destination (Join-Path $OutputDir 'io-trace.log') -Force
    $vfdLines = @(Get-Content -LiteralPath $ioTrace | Where-Object {
            $t = $_.Trim()
            if (-not $t) { return $false }
            try {
                $o = $t | ConvertFrom-Json
                $tag = [string]$o.tag
                $port = [string]$o.port
                return (($tag -eq 'vfd_data') -or ($tag -eq 'vfd_status') -or ($port -eq '0x0060'))
            }
            catch {
                return $false
            }
        })
    if ($vfdLines.Count -gt 0) {
        $vfdLines | Set-Content -LiteralPath $vfdTraceOut -Encoding UTF8
    }
    else {
        @{ metadata = $true; event = 'no_vfd_events_observed'; source = 'io-trace.jsonl'; note = 'No firmware VFD port 0x0060 events observed in this capture.' } |
            ConvertTo-Json -Compress |
            Set-Content -LiteralPath $vfdTraceOut -Encoding UTF8
    }
}
else {
    '(no io-trace.jsonl produced)' | Set-Content (Join-Path $OutputDir 'io-trace.log') -Encoding UTF8
    @{ metadata = $true; event = 'no_io_trace_produced'; source = 'io-trace.jsonl'; note = 'Cannot derive VFD trace because no I/O trace was produced.' } |
        ConvertTo-Json -Compress |
        Set-Content -LiteralPath $vfdTraceOut -Encoding UTF8
}

$unknownJson = Join-Path $OutputDir 'unknown-ports.json'
Convert-JsonlFileToJsonArray -Path $unknownPort -OutJson $unknownJson

$uartLog = Join-Path $OutputDir 'uart-transcript.log'
$ioText = if (Test-Path -LiteralPath $ioTrace) { Get-Content -LiteralPath $ioTrace -Raw } else { '' }
if ($ioText -match 'uart|UART|modem|MODEM|asci|ASCI') {
    $nl = [Environment]::NewLine
    (($ioText -split $nl) | Where-Object { $_ -match 'uart|UART|modem|MODEM|asci|ASCI' }) -join $nl | Set-Content -LiteralPath $uartLog -Encoding UTF8
}
else {
    '(no uart/modem lines in io-trace)' | Set-Content -LiteralPath $uartLog -Encoding UTF8
}

# --- Parse milestones / VFD text ---
$milestonesArr = @()
if (Test-Path -LiteralPath $bootTrace) {
    Get-Content -LiteralPath $bootTrace | ForEach-Object {
        $t = $_.Trim()
        if (-not $t) { return }
        try { $milestonesArr += ($t | ConvertFrom-Json) } catch { }
    }
}
$highestMs = ''
foreach ($o in $milestonesArr) {
    if ($o.milestone) { $highestMs = [string]$o.milestone }
}
$m6Obj = $milestonesArr | Where-Object { $_.milestone -eq 'M6' } | Select-Object -First 1
$m10ObjLegacy = $milestonesArr | Where-Object { $_.milestone -eq 'M10' } | Select-Object -First 1
$mDisplayIdleObj = $milestonesArr | Where-Object { $_.milestone -eq 'display_idle_heuristic' } | Select-Object -First 1
$mProtoObj = $milestonesArr | Where-Object { $_.milestone -eq 'protocol_ready' } | Select-Object -First 1
$mAcceptObj = $milestonesArr | Where-Object { $_.milestone -eq 'acceptance_ready' } | Select-Object -First 1
$m10ReachedObj = $milestonesArr | Where-Object { $_.milestone -in @('M10', 'display_idle_heuristic', 'protocol_ready', 'acceptance_ready') } | Select-Object -First 1
$m10Reached = ($null -ne $m10ReachedObj)
$m10Obj = $null
if ($m10ObjLegacy) { $m10Obj = $m10ObjLegacy }
elseif ($mDisplayIdleObj) { $m10Obj = $mDisplayIdleObj }
$vfdForNormal = ''
if ($mAcceptObj -and $mAcceptObj.vfd) { $vfdForNormal = [string]$mAcceptObj.vfd }
elseif ($mProtoObj -and $mProtoObj.vfd) { $vfdForNormal = [string]$mProtoObj.vfd }
elseif ($mDisplayIdleObj -and $mDisplayIdleObj.vfd) { $vfdForNormal = [string]$mDisplayIdleObj.vfd }
elseif ($m10ObjLegacy -and $m10ObjLegacy.vfd) { $vfdForNormal = [string]$m10ObjLegacy.vfd }
elseif ($m6Obj -and $m6Obj.vfd) { $vfdForNormal = [string]$m6Obj.vfd }
$bootNormalMilestoneTag = ''
if ($mAcceptObj) { $bootNormalMilestoneTag = 'acceptance_ready' }
elseif ($mProtoObj) { $bootNormalMilestoneTag = 'protocol_ready' }
elseif ($mDisplayIdleObj) { $bootNormalMilestoneTag = 'display_idle_heuristic' }
elseif ($m10ObjLegacy) { $bootNormalMilestoneTag = 'M10' }
elseif ($m6Obj) { $bootNormalMilestoneTag = 'M6' }
$vfdFinalTextPath = Join-Path $OutputDir 'vfd-final-text.txt'
$vfdFinalText = ''
if (Test-Path -LiteralPath $vfdFinalTextPath) {
    $vfdFinalText = (Get-Content -LiteralPath $vfdFinalTextPath -Raw)
}
$vfdFinalHasVisible = $false
if ($vfdFinalText) {
    $vfdFinalHasVisible = ($vfdFinalText -replace '[\s\r\n]', '').Length -gt 0
}

$bootNormalTxt = Join-Path $OutputDir 'boot-normal-start-vfd.txt'
$bootNormalJson = Join-Path $OutputDir 'boot-normal-start-vfd.json'
if ($vfdForNormal) {
    Set-Content -LiteralPath $bootNormalTxt -Value $vfdForNormal -Encoding UTF8
    @{ milestone = $bootNormalMilestoneTag; vfd_summary = $vfdForNormal } | ConvertTo-Json | Set-Content $bootNormalJson -Encoding UTF8
}
else {
    '(no M6/M10 VFD summary in boot-trace - firmware did not reach VFD milestone emission)' | Set-Content -LiteralPath $bootNormalTxt -Encoding UTF8
    '{}' | Set-Content -LiteralPath $bootNormalJson -Encoding UTF8
}

$adFrame1 = Join-Path $OutputDir 'ad-scroll-vfd-frame-1.txt'
$adFrame2 = Join-Path $OutputDir 'ad-scroll-vfd-frame-2.txt'
$adDiff = Join-Path $OutputDir 'ad-scroll-diff.txt'
if ($m6Obj -and $m10Obj -and $m6Obj.vfd -and $m10Obj.vfd -and ($m6Obj.vfd -ne $m10Obj.vfd)) {
    Set-Content -LiteralPath $adFrame1 -Value ([string]$m6Obj.vfd) -Encoding UTF8
    Set-Content -LiteralPath $adFrame2 -Value ([string]$m10Obj.vfd) -Encoding UTF8
    Set-Content -LiteralPath $adDiff -Value ("frame1:`n$($m6Obj.vfd)`n---`nframe2:`n$($m10Obj.vfd)") -Encoding UTF8
}
else {
    '(no two distinct VFD summaries in boot trace for scroll comparison)' | Set-Content -LiteralPath $adFrame1 -Encoding UTF8
    '(same)' | Set-Content -LiteralPath $adFrame2 -Encoding UTF8
    'ad_scroll_not_observed: boot trace did not contain differing M6 vs M10 VFD strings.' | Set-Content -LiteralPath $adDiff -Encoding UTF8
}

# GUI capture runs whenever we are not in headless (-video none) mode.
if ($captureBootOk) {
    $screenshotKind = 'real_gui_screenshot'
}
elseif ($vfdForNormal) {
    $screenshotKind = 'rendered_from_real_vfd_trace'
}
elseif ($Headless) {
    $screenshotKind = 'not_observed'
}
else {
    $screenshotKind = 'blocked'
}

if ((-not $captureBootOk) -and (-not $captureFinalOk) -and (-not $Headless)) {
    @"
# GUI screenshot status
real_gui_screenshot: failed - see capture-mame-screenshot.ps1 / window title MAME
"@ | Set-Content (Join-Path $OutputDir 'screenshot-capture-failed.md') -Encoding UTF8
}

if ((-not $captureBootOk) -and (-not $captureFinalOk) -and $vfdForNormal) {
    Write-VfdRenderedPng -Path (Join-Path $OutputDir 'vfd-rendered-normal-start.png') `
        -Heading 'rendered_from_real_vfd_trace - not a framebuffer grab' `
        -BodyLines $vfdForNormal `
        -FooterLabel 'Source: boot-trace.jsonl M6/M10 vfd field only when present.'
}

if ((-not $captureAd1Ok) -and $m6Obj -and $m10Obj -and ($m6Obj.vfd -ne $m10Obj.vfd)) {
    Write-VfdRenderedPng -Path (Join-Path $OutputDir 'vfd-rendered-post-install-ad-scroll.png') `
        -Heading 'rendered_from_real_vfd_trace - second milestone VFD summary' `
        -BodyLines ([string]$m10Obj.vfd) `
        -FooterLabel 'Compare ad-scroll-vfd-frame-*.txt; GUI capture failed or unavailable.'
}

if (-not ($m6Obj -and $m10Obj -and ($m6Obj.vfd -ne $m10Obj.vfd))) {
    @"
# Advertising scroll
status_label: ad_scroll_not_observed

Boot trace did not provide two distinct VFD summaries (M6 vs M10) within this run, so scrolling cannot be proven from trace evidence alone.

Next: extend I/O / timing until M6/M10 fire, or drive INSTALL/service via scenario runner with keypad verbs.

"@ | Set-Content (Join-Path $OutputDir 'ad-scroll-not-observed.md') -Encoding UTF8
}

# Scenario copies / results
$scenarioFixture = Join-Path $EmuRoot 'fixtures\scenarios\install-and-ad-scroll.json'
if (Test-Path -LiteralPath $scenarioFixture) {
    Copy-Item -LiteralPath $scenarioFixture -Destination (Join-Path $OutputDir 'scenario-install.json') -Force
}

$protocolGateHit = ($null -ne $mProtoObj)
$acceptanceGateHit = ($null -ne $mAcceptObj)
$idleHeuristicHit = (($null -ne $mDisplayIdleObj) -or ($null -ne $m10ObjLegacy))
$installScenarioReady = ($protocolGateHit -or $acceptanceGateHit -or $idleHeuristicHit)
$installBlocked = $true
$installAttempted = $false
$installCompleted = $false
$installMethod = 'not_observed'
if (-not $installScenarioReady) {
    $installMethod = 'install_blocked'
}

$scenarioResult = [ordered]@{
    scenario_id          = 'install-and-ad-scroll'
    status               = if ($installCompleted) { 'install_completed_firmware_driven' } else { 'blocked' }
    install_attempted    = $installAttempted
    install_completed    = $installCompleted
    install_method       = $installMethod
    m10_reached          = [bool]($null -ne $m10Reached)
    notes                = if ($RealInputDemo) { 'Real window keypress demo was executed via SendKeys on live MAME window.' } else { 'No scenario-runner verbs were executed in this run.' }
    firmware_install_reference = 'Observed install-sequence bytes 0x27,0x27,0x37,0x8E (mapped through keypad I/O profile)'
}
$scenarioResult | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $OutputDir 'scenario-install-result.json') -Encoding UTF8

# Build artifacts copy
$br = Join-Path $EmuRoot 'build\build-result.json'
if (Test-Path -LiteralPath $br) {
    Copy-Item -LiteralPath $br -Destination (Join-Path $OutputDir 'build-result.json') -Force
}
$mameLog = Join-Path $EmuRoot 'build\logs\mame-build.log'
if (Test-Path -LiteralPath $mameLog) {
    Get-Content -LiteralPath $mameLog -Tail 120 | Set-Content (Join-Path $OutputDir 'mame-build-tail.log') -Encoding UTF8
}

$unknownPortCount = 0
if (Test-Path -LiteralPath $unknownJson) {
    try {
        $up = Get-Content $unknownJson -Raw | ConvertFrom-Json
        if ($up -is [System.Array]) { $unknownPortCount = $up.Count }
    }
    catch {}
}

$ioTraceStatus = if ((Test-Path -LiteralPath $ioTrace) -and ((Get-Item $ioTrace).Length -gt 3)) { 'present' } else { 'not_observed' }

$vwStartDiag = Join-Path $OutputDir 'voiceware-startup-diagnostic.json'
$voicewUpdResolved = $false
$voicewRomBytes = $null
if (Test-Path -LiteralPath $vwStartDiag) {
    try {
        $vsj = Get-Content -LiteralPath $vwStartDiag -Raw | ConvertFrom-Json
        $voicewUpdResolved = [bool]$vsj.voicew_upd_finder_resolved
        $voicewRomBytes = $vsj.memory_region_voicew_bytes
    }
    catch {}
}

$m5vHit = $milestonesArr | Where-Object { $_.milestone -eq 'M5V' } | Select-Object -First 1
$m5aHit = $milestonesArr | Where-Object { $_.milestone -eq 'M5A' } | Select-Object -First 1
$m5cHit = $milestonesArr | Where-Object { $_.milestone -eq 'M5C' } | Select-Object -First 1
$b3Observed = $false
foreach ($o in $milestonesArr) {
    if ($o.milestone -eq 'M5V' -and $null -ne $o.phrase -and ([string]$o.phrase).ToUpperInvariant().Contains('B3')) {
        $b3Observed = $true
    }
}
$vwTraceFile = Join-Path $OutputDir 'voiceware-trace.jsonl'
if (Test-Path -LiteralPath $vwTraceFile) {
    Get-Content -LiteralPath $vwTraceFile | ForEach-Object {
        $tx = $_.Trim()
        if (-not $tx) { return }
        try {
            $xj = $tx | ConvertFrom-Json
            $val = [string]$xj.value
            $raw = [string]$xj.raw_sample_index
            if ($val -match 'B3' -or $raw -match 'B3') { $b3Observed = $true }
        }
        catch {}
    }
}

$wavPeakOut = $null
$wavNonSilent = $null
$wavExists = (Test-Path -LiteralPath $wavOutPath)
if ($wavExists -and $WavWrite) {
    try {
        $fsW = [System.IO.File]::OpenRead($wavOutPath)
        $brW = New-Object System.IO.BinaryReader($fsW)
        if ($fsW.Length -gt 44) {
            [void]$brW.ReadBytes(44)
            $maxAbs = 0
            while ($fsW.Position + 2 -le $fsW.Length) {
                $v = [math]::Abs([int]$brW.ReadInt16())
                if ($v -gt $maxAbs) { $maxAbs = $v }
            }
            $wavPeakOut = $maxAbs
            $wavNonSilent = ($maxAbs -gt 64)
        }
        $fsW.Close()
    }
    catch {
        $wavPeakOut = 'analysis_failed'
    }
}
$m5bFromWav = [bool]($wavNonSilent -eq $true)

[ordered]@{
    wav_path                     = $wavOutPath
    wav_requested                = [bool]$WavWrite
    wav_present                  = $wavExists
    peak_abs_int16               = $wavPeakOut
    audio_non_silent_heuristic   = $wavNonSilent
    m5b_non_silent_from_wav_peak = $m5bFromWav
    note                         = 'M5B is not a boot-trace milestone tag in current firmware; WAV peak classifies capture loudness only.'
} | ConvertTo-Json | Set-Content (Join-Path $OutputDir 'audio-capture-report.json') -Encoding UTF8

$phrase0xB3Report = [ordered]@{
    schema_version       = 'coinline.voiceware_phrase_probe/v1'
    command_observed     = if ($b3Observed) { '0xB3' } else { 'not_observed_this_run' }
    phrase_text_status   = 'unknown'
    confidence           = 'unknown'
    candidate_text       = @()
    audio_file           = if ($wavExists) { $wavOutPath } else { '' }
    reason               = 'Phrase text requires firmware/source/catalog proof; see docs/status/voiceware-phrase-0xB3-analysis.md'
}
$phrase0xB3Report | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $OutputDir 'voiceware-phrase-0xB3-report.json') -Encoding UTF8
$phrase0xB3Report | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $genDir 'voiceware-phrase-0xB3-report.json') -Encoding UTF8

$blockers = [System.Collections.Generic.List[string]]::new()
if (-not $installScenarioReady) {
    $blockers.Add('Boot readiness not observed: no protocol_ready, acceptance_ready, display_idle_heuristic, or legacy M10 milestone in boot-trace.jsonl.')
}
if ($ioTraceStatus -ne 'present') {
    $blockers.Add('io-trace.jsonl empty - firmware did not hit instrumented ports (0x60 VFD data / 0x63 PIO_PORT_G) during this run, or trace path unset.')
}
if ((-not $captureBootOk) -and (-not $captureFinalOk) -and (-not $Headless)) { $blockers.Add('GUI screenshot capture failed or window not found until CopyFromScreen fix is verified.') }
if (($captureFinalOk -eq $true) -and (-not $vfdFinalHasVisible)) { $blockers.Add('Final GUI screenshot exists but vfd-final-text.txt remained blank; render path mismatch suspected.') }

$shots = [ordered]@{
    boot_normal_start           = if (Test-Path (Join-Path $OutputDir 'boot-normal-start.png')) { (Resolve-Path (Join-Path $OutputDir 'boot-normal-start.png')).Path } else { $null }
    post_install_ad_scroll      = if (Test-Path (Join-Path $OutputDir 'post-install-ad-scroll.png')) { (Resolve-Path (Join-Path $OutputDir 'post-install-ad-scroll.png')).Path } else { $null }
    post_install_ad_scroll_2    = if (Test-Path (Join-Path $OutputDir 'post-install-ad-scroll-2.png')) { (Resolve-Path (Join-Path $OutputDir 'post-install-ad-scroll-2.png')).Path } else { $null }
    final_screen                = if (Test-Path (Join-Path $OutputDir 'final-screen.png')) { (Resolve-Path (Join-Path $OutputDir 'final-screen.png')).Path } else { $null }
    vfd_rendered_normal_start   = if (Test-Path (Join-Path $OutputDir 'vfd-rendered-normal-start.png')) { (Resolve-Path (Join-Path $OutputDir 'vfd-rendered-normal-start.png')).Path } else { $null }
    vfd_rendered_post_install    = if (Test-Path (Join-Path $OutputDir 'vfd-rendered-post-install-ad-scroll.png')) { (Resolve-Path (Join-Path $OutputDir 'vfd-rendered-post-install-ad-scroll.png')).Path } else { $null }
    screenshot_evidence_label   = $screenshotKind
}

$evidenceSummary = [ordered]@{
    trace_profile            = $TraceProfile
    real_input_demo_requested = [bool]$RealInputDemo
    real_input_demo_ran      = [bool]$realInputDemoRan
    firmware_sha256          = $fwSha
    emulator_executable      = (Resolve-Path $MameExe).Path
    mame_commit              = $mameCommit
    boot_milestone_highest   = $highestMs
    m10_reached              = [bool]$m10Reached
    protocol_ready_reached   = [bool]$protocolGateHit
    acceptance_ready_reached = [bool]$acceptanceGateHit
    display_idle_heuristic_reached = [bool]($null -ne $mDisplayIdleObj)
    voicew_upd_resolved      = $voicewUpdResolved
    voicew_region_bytes      = $voicewRomBytes
    voice_rom_layout         = $VoiceRomLayout
    voice_rom_u16_sha256     = $h16
    voice_rom_u26_sha256     = $h26
    command_0xB3_observed    = $b3Observed
    voiceware_milestones     = [ordered]@{
        M5V = [bool]($null -ne $m5vHit)
        M5A = [bool]($null -ne $m5aHit)
        M5B_non_silent_wav = $m5bFromWav
        M5C = [bool]($null -ne $m5cHit)
    }
    voiceware_output_wav     = if ($wavExists) { (Resolve-Path $wavOutPath).Path } else { '' }
    audio_non_silent         = $wavNonSilent
    phrase_0xB3_report       = if (Test-Path (Join-Path $OutputDir 'voiceware-phrase-0xB3-report.json')) { (Resolve-Path (Join-Path $OutputDir 'voiceware-phrase-0xB3-report.json')).Path } else { '' }
    normal_start_screen_observed = [bool]$vfdForNormal
    normal_start_text        = if ($vfdForNormal) { $vfdForNormal } else { 'not_observed' }
    vfd_final_text_present   = [bool]$vfdFinalHasVisible
    vfd_final_text_path      = if (Test-Path -LiteralPath $vfdFinalTextPath) { (Resolve-Path $vfdFinalTextPath).Path } else { '' }
    install_attempted        = $installAttempted
    install_completed        = $installCompleted
    install_method           = $installMethod
    ad_scroll_observed       = [bool]($m6Obj -and $m10Obj -and ($m6Obj.vfd -ne $m10Obj.vfd))
    screenshots              = $shots
    trace_files              = [ordered]@{
        run_log                  = (Resolve-Path $runLog).Path
        boot_trace               = if (Test-Path -LiteralPath $bootTrace) { (Resolve-Path $bootTrace).Path } else { '' }
        io_trace                 = if (Test-Path $ioTrace) { (Resolve-Path $ioTrace).Path } else { '' }
        vfd_trace                = if (Test-Path (Join-Path $OutputDir 'vfd-trace.jsonl')) { (Resolve-Path (Join-Path $OutputDir 'vfd-trace.jsonl')).Path } else { '' }
        unknown_ports            = (Resolve-Path $unknownJson).Path
        uart_transcript          = (Resolve-Path $uartLog).Path
        voiceware_roms           = (Resolve-Path (Join-Path $OutputDir 'voiceware-roms.json')).Path
        voiceware_region_report  = (Resolve-Path (Join-Path $OutputDir 'voiceware-region-report.json')).Path
        voiceware_startup_diag   = if (Test-Path $vwStartDiag) { (Resolve-Path $vwStartDiag).Path } else { '' }
        voiceware_trace          = if (Test-Path $vwTraceFile) { (Resolve-Path $vwTraceFile).Path } else { '' }
        audio_trace              = if (Test-Path $audioTracePath) { (Resolve-Path $audioTracePath).Path } else { '' }
        audio_route_trace        = if (Test-Path $routeTracePath) { (Resolve-Path $routeTracePath).Path } else { '' }
        mute_route_trace         = if (Test-Path $muteTracePath) { (Resolve-Path $muteTracePath).Path } else { '' }
        telephony_trace          = if (Test-Path $telTracePath) { (Resolve-Path $telTracePath).Path } else { '' }
        supervision_trace        = if (Test-Path $supTracePath) { (Resolve-Path $supTracePath).Path } else { '' }
        alerter_trace            = if (Test-Path $altTracePath) { (Resolve-Path $altTracePath).Path } else { '' }
        audio_capture_report     = (Resolve-Path (Join-Path $OutputDir 'audio-capture-report.json')).Path
    }
    unknown_ports_count      = $unknownPortCount
    io_trace_status          = $ioTraceStatus
    blockers                 = $blockers
    next_task                = 'Align TP board readiness with CP CSI/O path until acceptance_ready or protocol_ready fires; keep INSTALL proof on TP keypad traces and craft-entry gate, not PIO/VFD substrings alone.'
}
$evidenceSummary | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $OutputDir 'evidence-summary.json') -Encoding UTF8

$enabledTraces = @('boot-milestones.json', 'evidence-summary.json', 'vfd-trace.jsonl', 'io-trace.jsonl', 'boot-blocker.md')
$disabledTraces = @('memory-trace.jsonl', 'stack-trace.jsonl', 'mmu-translation-trace.jsonl', 'audio-trace.jsonl', 'telephony-trace.jsonl', 'mute-route-trace.jsonl')
if ($TraceProfile -eq 'full') {
    $enabledTraces += $disabledTraces
    $disabledTraces = @()
}
if ($TraceProfile -eq 'm6' -or $TraceProfile -eq 'full') {
    $enabledTraces += @('cpu-trace.jsonl', 'hot-pc-frequency.json', 'hot-port-frequency.json')
} else {
    $disabledTraces += @('cpu-trace.jsonl')
}
[ordered]@{
    selected_profile      = $TraceProfile
    enabled_trace_files   = $enabledTraces
    disabled_trace_files  = $disabledTraces
    run_seconds           = $RunSeconds
    mame_executable_sha256= (Get-CoinlineSha256 $MameExe)
    firmware_sha256       = $fwSha
    audio_enabled         = [bool]$EnableAudio
    wav_enabled           = [bool]$WavWrite
} | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $OutputDir 'trace-profile-report.json') -Encoding UTF8

$evMd = @()
$evMd += '# Evidence summary (screenshot capture)'
$evMd += ''
$evMd += "- Firmware SHA256: ``$fwSha``"
$evMd += "- Emulator: ``$MameExe``"
$evMd += "- MAME commit: ``$mameCommit``"
$evMd += "- Highest milestone: **$highestMs**; M10: **$(if ($m10Reached) { 'yes' } else { 'no' })**"
$evMd += "- Screenshot label: **$screenshotKind** (GUI used: **$guiUsed**)"
$evMd += "- INSTALL: **install_blocked** / not attempted (no runner keypad injection in this harness)"
$evMd += "- Ad scroll (trace-based): **$(if ($evidenceSummary.ad_scroll_observed) { 'ad_scroll_observed' } else { 'ad_scroll_not_observed' })**"
$evMd += "- Blockers: $($blockers -join ' | ')"
$evMd += ''
$evMd | Set-Content (Join-Path $OutputDir 'evidence-summary.md') -Encoding UTF8

$report = @()
$report += '# Screenshot capture report'
$report += ''
$report += "## Firmware"
$report += "- Path: ``$($inputRes.firmware_binary_absolute)``"
$report += "- SHA256: ``$fwSha``"
$report += ''
$report += "## Emulator"
$report += "- Executable: ``$MameExe``"
$report += "- MAME commit: ``$mameCommit``"
$report += "- Run command: ``$MameExe $($mameArgs -join ' ')``"
$report += "- MAME GUI: **$(if ($guiUsed) { "yes ($video window)" } else { 'no (-video none)' })**"
$report += "- Screenshot evidence type: **$screenshotKind** - GUI PNG is primary when present; **vfd-rendered-*.png** is labeled **rendered_from_real_vfd_trace** and is not a live framebuffer grab."
$report += "- Final GUI screenshot: **$(if ($captureFinalOk) { 'captured (final-screen.png)' } else { 'not_captured' })**"
$report += "- Final VFD text file: **$(if ($vfdFinalHasVisible) { 'non-empty' } else { 'empty_or_missing' })**"
$report += ''
$report += "## Boot milestones"
$report += "- Highest: **$highestMs**"
$report += "- M10 reached: **$(if ($m10Reached) { 'yes' } else { 'no' })**"
$report += '- Initial/normal VFD text from trace: ' + $(if ($vfdForNormal) { '``' + $vfdForNormal + '``' } else { '**not_observed** (no M6/M10 vfd field)' })
$report += ''
$report += "## INSTALL / advertising"
$report += "- INSTALL: **install_blocked** - scenario runner not executed; no firmware-visible keypad sequence was injected during this capture."
$report += "- INSTALL reference (source only): observed parameter bytes ``27 27 37 8E`` used by install entry path."
$report += "- Advertising scroll: **$(if ($evidenceSummary.ad_scroll_observed) { 'ad_scroll_observed' } else { 'ad_scroll_not_observed' })** (proof requires differing VFD summaries or GUI frames)."
$report += ''
$report += "## Traces"
$report += "- ``run.log``, ``boot-trace.jsonl``, ``boot-milestones.json``"
$report += "- ``io-trace.log`` / ``io-trace.jsonl``: **$ioTraceStatus** (driver may not yet emit I/O JSONL)"
$report += "- ``unknown-ports.json`` ($unknownPortCount entries)"
$report += "- ``vfd-trace.jsonl`` (filtered from io-trace when present)"
$report += ''
$report += "## Blockers / next implementation task"
$report += $(if ($blockers.Count) { ($blockers | ForEach-Object { "- $_" }) } else { '- (none listed)' })
$report += '- **Next:** extend CoinLine I/O until **M6/M10** appear in ``boot-trace.jsonl``; implement **COINLINE_IO_TRACE** emission in the driver; wire **install-and-ad-scroll** scenario through the scenario runner with keypad verbs.'
$report += ''
$report | Set-Content (Join-Path $OutputDir 'screenshot-capture-report.md') -Encoding UTF8

Write-Host "Screenshot capture phase complete. OutputDir=$OutputDir exit=$($p.ExitCode) gracefulStop=$gracefulStop forcedStop=$forcedStop"
exit 0

