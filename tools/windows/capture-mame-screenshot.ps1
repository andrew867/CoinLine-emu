# SPDX-License-Identifier: GPL-2.0-or-later
#requires -Version 5.1
param(
    [Parameter(Mandatory = $true)][string] $RunDir,
    [string] $WindowTitleSubstring = '',
    [string] $OutputPath = ''
)

$ErrorActionPreference = 'Stop'

$sig = @'
using System;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
public class MameWinCap {
  [StructLayout(LayoutKind.Sequential)]
  public struct RECT { public int Left, Top, Right, Bottom; }
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
  static bool TitleLooksLikeMame(string t) {
    if (string.IsNullOrEmpty(t)) return false;
    return t.IndexOf("MAME", StringComparison.OrdinalIgnoreCase) >= 0
        || t.IndexOf("CoinLine", StringComparison.OrdinalIgnoreCase) >= 0
        || t.IndexOf("coinline", StringComparison.OrdinalIgnoreCase) >= 0
        || t.IndexOf("Millennium", StringComparison.OrdinalIgnoreCase) >= 0
        || t.IndexOf("cl_millennium", StringComparison.OrdinalIgnoreCase) >= 0;
  }
  static bool ExeLooksLikeCoinlineMame(Process p) {
    try {
      string fp = p.MainModule.FileName;
      return fp.IndexOf("coinline-mame", StringComparison.OrdinalIgnoreCase) >= 0;
    } catch { return false; }
  }
  public static void SaveBest(string titleSub, string path) {
    IntPtr bestHwnd = IntPtr.Zero;
    RECT bestRect = new RECT();
    int bestArea = 0;
    foreach (var p in Process.GetProcesses()) {
      try {
        if (p.MainWindowHandle == IntPtr.Zero) continue;
        bool ok = ExeLooksLikeCoinlineMame(p);
        if (!ok && !string.IsNullOrEmpty(titleSub)) {
          try {
            if (!string.IsNullOrEmpty(p.MainWindowTitle) && p.MainWindowTitle.IndexOf(titleSub, StringComparison.OrdinalIgnoreCase) >= 0)
              ok = true;
          } catch {}
        }
        if (!ok && TitleLooksLikeMame(p.MainWindowTitle))
          ok = true;
        if (!ok) continue;
        RECT r;
        if (!GetWindowRect(p.MainWindowHandle, out r)) continue;
        int w = r.Right - r.Left, h = r.Bottom - r.Top;
        if (w < 8 || h < 8) continue;
        int area = w * h;
        if (area > bestArea) {
          bestArea = area;
          bestHwnd = p.MainWindowHandle;
          bestRect = r;
        }
      } catch {}
    }
    if (bestHwnd == IntPtr.Zero) return;
    int bw = bestRect.Right - bestRect.Left, bh = bestRect.Bottom - bestRect.Top;
    using (var bmp = new Bitmap(bw, bh)) {
      using (var g = Graphics.FromImage(bmp)) {
        g.CopyFromScreen(bestRect.Left, bestRect.Top, 0, 0, new Size(bw, bh));
      }
      bmp.Save(path, ImageFormat.Png);
    }
  }
}
'@
try {
    Add-Type -TypeDefinition $sig -ReferencedAssemblies 'System.Drawing' -ErrorAction Stop
}
catch {
    Write-Warning "Could not compile screenshot helper: $_"
    exit 0
}

$out = if ($OutputPath) { $OutputPath } else { Join-Path $RunDir 'screenshot.png' }
try {
    [MameWinCap]::SaveBest($WindowTitleSubstring, $out)
    if (Test-Path -LiteralPath $out) {
        Write-Host "Wrote $out"
    }
    else {
        Write-Warning "No CoinLine/MAME window found for screenshot."
    }
}
catch {
    Write-Warning "Screenshot failed: $_"
}
exit 0
