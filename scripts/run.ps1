<#
.SYNOPSIS  Run the recompiled game with logs in .\logs.  .\scripts\run.ps1 [-Preset win-x64-release] [-Seconds 0]
.NOTES     Any RECOMP_* variable you set (see tools\xboxrecomp\docs\runtime\environment-variables.md) is passed through.
#>
param([string]$Preset = "win-x64-release", [int]$Seconds = 0)
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")
$exe = Join-Path $RepoRoot "build\$Preset\turok_recomp.exe"
if (-not (Test-Path $exe)) { throw "Build first: .\scripts\build.ps1 -Preset $Preset" }
if (-not $env:TUROK_GAME_DIR) { $env:TUROK_GAME_DIR = $Extracted }
$logs = Join-Path $RepoRoot "logs"; New-Item -ItemType Directory -Force $logs | Out-Null
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$out = Join-Path $logs "$stamp.out.log"; $err = Join-Path $logs "$stamp.err.log"
$p = Start-Process $exe -WorkingDirectory $RepoRoot -RedirectStandardOutput $out -RedirectStandardError $err -PassThru
if ($Seconds -gt 0) {
    if (-not $p.WaitForExit($Seconds * 1000)) { Stop-Process -Id $p.Id -Force; Write-Host "(stopped after $Seconds s)" }
} else { $p.WaitForExit() }
Write-Host "exit: $($p.ExitCode)"
Write-Host "stdout: $out"
Write-Host "stderr: $err"
