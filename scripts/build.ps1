<#
.SYNOPSIS  Configure + build.  .\scripts\build.ps1 [-Preset win-x64-release|win-x64-debug] [-Clean]
#>
param([string]$Preset = "win-x64-release", [switch]$Clean)
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")
. (Join-Path $PSScriptRoot "env.ps1")
Push-Location $RepoRoot
try {
    if ($Clean) { Remove-Item -LiteralPath (Join-Path $RepoRoot "build\$Preset") -Recurse -Force -ErrorAction SilentlyContinue }
    Get-Process turok_recomp -ErrorAction SilentlyContinue | Stop-Process -Force
    cmake --preset $Preset; if ($LASTEXITCODE) { throw "configure failed" }
    cmake --build --preset $Preset 2>&1 | Tee-Object -FilePath (Join-Path $RepoRoot "build\last-build.log")
    if ($LASTEXITCODE) { throw "build failed (see build\last-build.log)" }
    Write-Host "Built build\$Preset\turok_recomp.exe. Next: .\scripts\run.ps1"
} finally { Pop-Location }
