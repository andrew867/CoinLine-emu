# SPDX-License-Identifier: GPL-2.0-or-later
#requires -Version 5.1
# Thin wrapper — canonical script lives in tools/windows/
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
& (Join-Path $here 'windows\validate-boot-milestones.ps1') @args
exit $LASTEXITCODE
