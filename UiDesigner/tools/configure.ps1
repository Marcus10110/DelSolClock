# Configure build directories for Debug and Release configurations
param(
  [ValidateSet("Debug", "Release")]
  [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"

# Set up paths - separate directories for Debug/Release
$BuildDir = "build\$($Config.ToLower())"
$depsFolder = ".\.deps"  # The folder where ninja.exe is located

Write-Host "Configuring $Config build in $BuildDir..." -ForegroundColor Cyan

# Ensure the ninja binary is in the ./.deps folder
$ninjaPath = Join-Path $depsFolder "ninja.exe"

# Check if ninja.exe exists in the specified location
if (-Not (Test-Path $ninjaPath)) {
    Write-Host "ninja.exe not found in $depsFolder" -ForegroundColor Red
    Write-Host "Please download ninja.exe and place it in $depsFolder" -ForegroundColor Yellow
    exit 1
}

# Initialize Visual Studio environment
. "$PSScriptRoot\vsenv.ps1"

# Create build directory if it doesn't exist
if (!(Test-Path $BuildDir)) {
  New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
  Write-Host "Created build directory: $BuildDir" -ForegroundColor Green
}

# Configure CMake (force reconfigure if build.ninja doesn't exist or CMakeLists.txt is newer)
$cmakeListsPath = "CMakeLists.txt"
$buildNinjaPath = Join-Path $BuildDir "build.ninja"
$needsReconfigure = $false

if (!(Test-Path $buildNinjaPath)) {
    $needsReconfigure = $true
    Write-Host "build.ninja not found, configuring..." -ForegroundColor Yellow
} elseif ((Get-Item $cmakeListsPath).LastWriteTime -gt (Get-Item $buildNinjaPath).LastWriteTime) {
    $needsReconfigure = $true
    Write-Host "CMakeLists.txt is newer than build.ninja, reconfiguring..." -ForegroundColor Yellow
}

if ($needsReconfigure) {
    Write-Host "Running CMake configuration..." -ForegroundColor Yellow
    cmake -S . -B $BuildDir -G Ninja -DCMAKE_BUILD_TYPE="$Config"
    if ($LASTEXITCODE -ne 0) {
        Write-Error "CMake configuration failed with exit code $LASTEXITCODE"
        exit $LASTEXITCODE
    }
    Write-Host "CMake configuration completed successfully." -ForegroundColor Green
} else {
    Write-Host "Build configuration is up to date." -ForegroundColor Green
}

Write-Host "Configuration complete for $Config build." -ForegroundColor Cyan