# Boot-critical runbook

## Build (Windows)

```powershell
& '.\tools\windows\overlay-coinline-driver.ps1'
& '.\tools\windows\build-mame-coinline.ps1' -MsysRoot 'C:\msys64'
```

## 180 s traced capture

```powershell
$ts = Get-Date -Format 'yyyyMMddTHHmmss'
$out = Join-Path '.\build\runs' ($ts + '-boot-critical')
New-Item -ItemType Directory -Force -Path $out | Out-Null

$env:COINLINE_TRACE_STACK='1'
$env:COINLINE_TRACE_RAM_INIT='1'
$env:COINLINE_TRACE_MMU_TRANSLATION='1'
$env:COINLINE_TRACE_INTERRUPTS='1'
$env:COINLINE_TRACE_TIMERS='1'
$env:COINLINE_TRACE_ASCI='1'
$env:COINLINE_TRACE_RESET='1'
$env:COINLINE_TRACE_VOICEWARE='1'
$env:COINLINE_TRACE_AUDIO='1'
$env:COINLINE_TRACE_AUDIO_ROUTE='1'
$env:COINLINE_TRACE_TELEPHONY='1'
$env:COINLINE_TRACE_MUTE='1'
$env:COINLINE_TRACE_VECTOR_EVENTS='1'

& '.\tools\windows\run-screenshot-capture.ps1' `
  -FirmwareBinary '..\firmware\flash.bin' `
  -FirmwareSourceRoot '..\firmware' `
  -VoiceRomU16 '..\firmware\voice_a.bin' `
  -VoiceRomU26 '..\firmware\voice_b.bin' `
  -VoiceRomLayout 'banked_two_roms' `
  -HostUrl 'http://127.0.0.1:5000' `
  -RunSeconds 180 `
  -OutputDir $out `
  -Windowed $true `
  -Screenshot `
  -EnableAudio `
  -WavWrite `
  -AudioTrace `
  -VoicewareTrace

& '.\tools\windows\validate-boot-milestones.ps1' -RunDir $out
```

## Compare baseline

Diff **hot-pc**, **hot-port**, `post-mmu-boot-loop-signature.json` (if present) against:

`build/runs/20260504T135115-post-mmu-boot-gate`

## After run

1. Update [`boot-critical-final-status.md`](boot-critical-final-status.md)  
2. Update [`docs/status/boot-milestone-status.json`](../status/boot-milestone-status.json)  
3. Update [`hardware-gap-register.json`](hardware-gap-register.json) if status changed  
4. Commit if behavior or milestone changed

## Blocker analysis (if M6 missing)

1. Top hot PCs — `hot-pc-frequency.json`  
2. Top hot ports — `hot-port-frequency.json`  
3. Loop signature — `post-mmu-boot-loop-signature.json`  
4. Gate hypothesis: INT vs ASCI vs MACH vs voice vs NVRAM vs unknown  
5. One **trace- or fixture-backed** code fix; rebuild; rerun — no “investigate only” exit.
