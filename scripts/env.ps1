# Dot-source to get MSVC + the CMake/Ninja bundled with Visual Studio on PATH:
#   . .\scripts\env.ps1
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found - install Visual Studio 2022 (Build Tools is enough) with the C++ workload" }
$vs = & $vswhere -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -sort -latest -property installationPath
if (-not $vs) { throw "No Visual Studio with the C++ x64 toolset found" }
$devcmd = Join-Path $vs "Common7\Tools\VsDevCmd.bat"
cmd /c "`"$devcmd`" -arch=amd64 -host_arch=amd64 -no_logo && set" | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') { Set-Item -Path "env:$($matches[1])" -Value $matches[2] }
}
$env:PATH = (Join-Path $vs "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin") + ";" +
            (Join-Path $vs "Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja") + ";" + $env:PATH
Write-Host ("cl:    " + (Get-Command cl.exe).Source)
Write-Host ("cmake: " + (cmake --version | Select-Object -First 1))
