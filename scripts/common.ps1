# Shared paths. Dot-sourced by the other scripts.
# TUROK_DATA holds everything that comes from YOUR disc (never the repo):
#   $TUROK_DATA\extracted   the unpacked disc (default.xbe, supertree*.tre, Data\)
# Default: a "turok" folder next to the repo.
$RepoRoot   = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$ToolkitDir = Join-Path $RepoRoot "tools\xboxrecomp"
$DataDir    = if ($env:TUROK_DATA) { $env:TUROK_DATA } else { Join-Path (Split-Path $RepoRoot -Parent) "turok" }
$Extracted  = Join-Path $DataDir "extracted"
$GenDir     = Join-Path $RepoRoot "src\recomp\gen"

# Python 3.10+: the Windows launcher if present, else python on PATH.
$PyExe = "python"; $PyPre = @()
if (Get-Command py -ErrorAction SilentlyContinue) { $PyExe = "py"; $PyPre = @("-3") }
elseif (-not (Get-Command python -ErrorAction SilentlyContinue)) { throw "Python 3.10+ not found" }

function Invoke-Py {
    & $PyExe @PyPre @args
    if ($LASTEXITCODE) { throw "python $($args -join ' ') failed (exit $LASTEXITCODE)" }
}
