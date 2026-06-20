# Build the screen renderer using CMake + Ninja + MSVC
param(
  [ValidateSet("Debug", "Release")]
  [string]$Config = "Debug",
  [string]$Target = "render_screens",
  [switch]$Configure,
  [switch]$Run
)

$ErrorActionPreference = "Stop"

# Set up paths - separate directories for Debug/Release  
$BuildDir = "build\$($Config.ToLower())"

Write-Host "Building $Config configuration..." -ForegroundColor Cyan

# Configure first if requested or if build directory doesn't exist
if ($Configure -or !(Test-Path $BuildDir)) {
    Write-Host "Configuring build..." -ForegroundColor Yellow
    & "$PSScriptRoot\configure.ps1" -Config $Config
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Configuration failed"
        exit $LASTEXITCODE
    }
}

# Build
Write-Host "Building target '$Target'..." -ForegroundColor Yellow
cmake --build $BuildDir --config $Config --target $Target

if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed with exit code $LASTEXITCODE"
    exit $LASTEXITCODE
}

Write-Host "Build completed successfully." -ForegroundColor Green

# Run if requested
if ($Run) {
    $exe = Join-Path $BuildDir "$Target.exe"
    if (Test-Path $exe) {
        Write-Host "Running $exe..." -ForegroundColor Yellow
        $fullPath = Resolve-Path $exe
        pushd (Get-Location)  # Stay in project root directory
        & $fullPath
        $exitCode = $LASTEXITCODE
        popd
        if ($exitCode -ne 0) {
            Write-Error "Execution failed with exit code $exitCode"
            exit $exitCode
        }
        Write-Host "Execution completed successfully." -ForegroundColor Green
    } else {
        Write-Error "Executable not found: $exe"
        exit 1
    }
}
