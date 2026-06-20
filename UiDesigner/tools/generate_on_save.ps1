# Fast build and run for screen generation (Release only)
# Used by VS Code "Run on Save" extension for quick iteration

param(
  [string]$Target = "render_screens"
)

$ErrorActionPreference = "Stop"
$Config = "Release"  # Always use Release for screen generation
$BuildDir = "build\release"

# Anchor to the UiDesigner dir (this script lives in UiDesigner/tools), so it
# works whether invoked from the repo root (run-on-save) or from UiDesigner/.
# render_screens.exe resolves data/ + out/ relative to this CWD.
Set-Location (Join-Path $PSScriptRoot "..")

# Initialize Visual Studio environment
. "$PSScriptRoot\vsenv.ps1"

Write-Host "Generating screens (Release build)..." -ForegroundColor Cyan

# Quick build - assume configuration is already done
# If build directory doesn't exist, configure it first
if (!(Test-Path $BuildDir)) {
    Write-Host "Release build not configured, configuring..." -ForegroundColor Yellow
    & "$PSScriptRoot\configure.ps1" -Config Release
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Configuration failed"
        exit $LASTEXITCODE
    }
}

# Build only if needed (incremental)
Write-Host "Building..." -ForegroundColor Yellow
cmake --build $BuildDir --config $Config --target $Target

if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed with exit code $LASTEXITCODE"
    exit $LASTEXITCODE
}

# Run screen generation
$exe = Join-Path $BuildDir "render_screens.exe"
if (Test-Path $exe) {
    Write-Host "Generating screen images..." -ForegroundColor Yellow
    pushd (Get-Location)  # Stay in project root for correct relative paths
    & $exe
    $exitCode = $LASTEXITCODE
    popd

    if ($exitCode -eq 0) {
        Write-Host "Screen generation completed successfully." -ForegroundColor Green

        # Show generated files
        if (Test-Path "out") {
            $bmps = Get-ChildItem -Path "out" -Filter "*.bmp" | Measure-Object
            Write-Host "Generated $($bmps.Count) BMP file(s) in ./out/" -ForegroundColor Green
        }
    } else {
        Write-Error "Screen generation failed with exit code $exitCode"
        exit $exitCode
    }
} else {
    Write-Error "render_screens.exe not found: $exe"
    exit 1
}
