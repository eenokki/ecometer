# build.ps1 — Green Benchmark Emission Analyzer builder
# Run from ecometer folder as administrator

param(
    [switch]$Clean = $false
)

$ErrorActionPreference = "Stop"

# Clean build if requested or forced
if ($Clean -or !(Test-Path "build")) {
    Write-Host "`n[1/3] Cleaning old build..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
    
    Write-Host "[2/3] Configuring with CMake..." -ForegroundColor Yellow
    cmake -B build -S . -A x64
    if ($LASTEXITCODE -ne 0) { Write-Host "CMake configure failed" -ForegroundColor Red; exit 1 }

    # Patch Spectre requirement out of PCM_STATIC
    Write-Host "      Patching PCM Spectre requirement..." -ForegroundColor Gray
    $vcxproj = "build\pcm\src\PCM_STATIC.vcxproj"
    if (Test-Path $vcxproj) {
        (Get-Content $vcxproj) `
            -replace '<SpectreMitigation>Spectre</SpectreMitigation>', '<SpectreMitigation>false</SpectreMitigation>' |
            Set-Content $vcxproj
        Write-Host "      Patched OK" -ForegroundColor Gray
    }
} else {
    Write-Host "`n[1/3] Using existing build folder (run with -Clean to rebuild)" -ForegroundColor Gray
    Write-Host "[2/3] Skipping CMake configure..." -ForegroundColor Gray
}

# Build
Write-Host "[3/3] Building Release..." -ForegroundColor Yellow
cmake --build build --config Release
if ($LASTEXITCODE -ne 0) { Write-Host "Build failed" -ForegroundColor Red; exit 1 }

Write-Host "`nBuild successful!" -ForegroundColor Green
Write-Host "Executable: build\Release\ecometer.exe" -ForegroundColor Green
Write-Host "`nUsage:" -ForegroundColor Cyan
Write-Host "  .\build\Release\ecometer.exe <program.exe>" -ForegroundColor White
Write-Host "  .\build\Release\ecometer.exe <program.exe> <param>" -ForegroundColor White