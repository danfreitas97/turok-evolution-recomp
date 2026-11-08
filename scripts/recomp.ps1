<#
.SYNOPSIS  Lift default.xbe to C into src\recomp\gen (run analyze.ps1 first, ~2 min).
#>
param([int]$Split = 1000)
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")
if (-not (Test-Path (Join-Path $ToolkitDir "tools\abi_analysis\output\abi_functions.json"))) { throw "Run scripts\analyze.ps1 first" }
New-Item -ItemType Directory -Force $GenDir | Out-Null
Get-ChildItem $GenDir -File | Where-Object Name -ne ".gitkeep" | Remove-Item
Push-Location $ToolkitDir
try { Invoke-Py -m tools.recomp game_files/default.xbe --all --split $Split --gen-dir $GenDir } finally { Pop-Location }
Write-Host ("Generated: " + (Get-ChildItem $GenDir -Filter *.c).Count + " C files, " +
    [math]::Round((Get-ChildItem $GenDir | Measure-Object Length -Sum).Sum/1MB) + " MB")
Write-Host "Next: .\scripts\build.ps1"
