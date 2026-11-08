<#
.SYNOPSIS  Unpack YOUR Turok: Evolution disc image into $TUROK_DATA\extracted and link .\game to it.
.EXAMPLE   .\scripts\extract-disc.ps1 -Image "D:\Games\Xbox Original\Turok - Evolution (USA).xiso.iso"
.NOTES     Accepts Redump (full disc) and XISO images. Uses the toolkit's own xdvdfs reader.
#>
param([Parameter(Mandatory)] [string]$Image)
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")
$Image = (Resolve-Path $Image).Path
New-Item -ItemType Directory -Force $Extracted | Out-Null
Push-Location $ToolkitDir
try { Invoke-Py -m tools.xiso unpack $Image -o $Extracted } finally { Pop-Location }
Invoke-Py (Join-Path $PSScriptRoot "verify-dump.py") $Extracted
$link = Join-Path $RepoRoot "game"
if (-not (Test-Path $link)) { New-Item -ItemType Junction -Path $link -Target $Extracted | Out-Null }
Write-Host "game -> $Extracted"
Write-Host "Next: .\scripts\analyze.ps1"
