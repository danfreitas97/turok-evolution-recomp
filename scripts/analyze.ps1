<#
.SYNOPSIS  xbe_parser -> disasm -> func_id -> abi_analysis on your default.xbe (Python only, ~2 min).
#>
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")
Invoke-Py (Join-Path $PSScriptRoot "verify-dump.py") $Extracted
New-Item -ItemType Directory -Force (Join-Path $ToolkitDir "game_files") | Out-Null
Copy-Item (Join-Path $Extracted "default.xbe") (Join-Path $ToolkitDir "game_files\default.xbe") -Force
Push-Location $ToolkitDir
try {
    Invoke-Py -m tools.xbe_parser game_files/default.xbe --json game_files/default_analysis.json
    # Every x86 section: the game's .text plus the statically linked XDK
    # libraries. DOLBY is DSP microcode, not x86, so it is left out.
    Invoke-Py -m tools.disasm game_files/default.xbe --force --extra-sections D3D,D3DX,DSOUND,XPP,XGRPH,XMV
    Invoke-Py -m tools.func_id game_files/default.xbe -v
    Invoke-Py -m tools.abi_analysis game_files/default.xbe -v
} finally { Pop-Location }
Write-Host "Analysis done. Next: .\scripts\recomp.ps1"
