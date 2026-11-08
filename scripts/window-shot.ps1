<#
.SYNOPSIS  Save a PNG of the running game's window.  .\scripts\window-shot.ps1 [-Out logs\shot.png]
.NOTES     Uses PrintWindow, so the window may be behind others. Captures stay local
           (logs\ is gitignored): a frame of the game is the game's artwork.
#>
param([string]$Out = "", [string]$Process = "turok_recomp")
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")
if (-not $Out) {
    New-Item -ItemType Directory -Force (Join-Path $RepoRoot "logs") | Out-Null
    $Out = Join-Path $RepoRoot ("logs\shot-" + (Get-Date -Format "yyyyMMdd-HHmmss") + ".png")
}
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class TurokShot {
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
    public delegate bool EnumProc(IntPtr h, IntPtr p);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc f, IntPtr p);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint flags);
    public static IntPtr Find(uint pid) {
        IntPtr best = IntPtr.Zero; long area = 0;
        EnumWindows((h, p) => {
            uint wp; GetWindowThreadProcessId(h, out wp);
            if (wp == pid && IsWindowVisible(h)) {
                RECT r; GetClientRect(h, out r);
                long a = (long)(r.R - r.L) * (r.B - r.T);
                if (a > area) { area = a; best = h; }
            }
            return true;
        }, IntPtr.Zero);
        return best;
    }
}
"@
$p = Get-Process $Process -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $p) { throw "$Process is not running" }
$h = [TurokShot]::Find([uint32]$p.Id)
if ($h -eq [IntPtr]::Zero) { throw "no visible window for $Process" }
$r = New-Object TurokShot+RECT
[void][TurokShot]::GetClientRect($h, [ref]$r)
$w = $r.R - $r.L; $hh = $r.B - $r.T
$bmp = New-Object System.Drawing.Bitmap $w, $hh
$g = [System.Drawing.Graphics]::FromImage($bmp)
$dc = $g.GetHdc()
[void][TurokShot]::PrintWindow($h, $dc, 3)   # PW_CLIENTONLY | PW_RENDERFULLCONTENT
$g.ReleaseHdc($dc); $g.Dispose()
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
Write-Host "$Out (${w}x${hh})"
