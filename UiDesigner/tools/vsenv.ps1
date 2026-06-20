# Import the Visual Studio 2022 build environment into the current PowerShell
# session (so cl/cmake/ninja resolve standard headers + the x64 toolchain).
# Dot-source this: . "$PSScriptRoot\vsenv.ps1"
#
# Locates VS via vswhere (works for Community/Professional/Enterprise) and runs
# VsDevCmd.bat for x64, importing the resulting environment variables.

$ErrorActionPreference = "Stop"

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    Write-Error "vswhere.exe not found; is Visual Studio 2022 installed?"
    exit 1
}

$vsRoot = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath
if (-not $vsRoot) {
    Write-Error "No Visual Studio install with the C++ toolchain was found."
    exit 1
}

$vsDevCmd = Join-Path $vsRoot "Common7\Tools\VsDevCmd.bat"
if (-not (Test-Path $vsDevCmd)) {
    Write-Error "VsDevCmd.bat not found at $vsDevCmd"
    exit 1
}

Write-Host "Initializing VS environment ($vsRoot)..." -ForegroundColor Yellow
# Run VsDevCmd for x64 and import every resulting env var into this session.
cmd /c "`"$vsDevCmd`" -arch=x64 -host_arch=x64 && set" |
    ForEach-Object {
        if ($_ -match "^([^=]+)=(.*)$") {
            Set-Item -Force -Path "ENV:\$($matches[1])" -Value $matches[2]
        }
    }
Write-Host "VS environment ready." -ForegroundColor Green
