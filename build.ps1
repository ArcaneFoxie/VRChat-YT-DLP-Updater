# PowerShell build script for yt-dlp updater

# Set error action preference to stop on any error
$ErrorActionPreference = "Stop"

# Function to check if a command exists
function Test-CommandExists {
    param ($command)
    $oldPreference = $ErrorActionPreference
    $ErrorActionPreference = 'stop'
    try { if (Get-Command $command) { return $true } }
    catch { return $false }
    finally { $ErrorActionPreference = $oldPreference }
}

# Function to check if a package is installed via vcpkg
function Test-VcpkgPackageInstalled {
    param ($package)
    $oldPreference = $ErrorActionPreference
    $ErrorActionPreference = 'stop'
    try { 
        $result = & vcpkg list | Select-String -Pattern "^$package\s+" 
        return $result -ne $null 
    }
    catch { return $false }
    finally { $ErrorActionPreference = $oldPreference }
}

# Function to find vcpkg using VCPKG_ROOT
function Find-VcpkgPath {
    # Check VCPKG_ROOT environment variable
    if ($env:VCPKG_ROOT) {
        if (Test-Path "$env:VCPKG_ROOT\vcpkg.exe") {
            return $env:VCPKG_ROOT
        }
    }
    
    # Check if vcpkg is in PATH
    if (Test-CommandExists "vcpkg") {
        $vcpkgPath = (Get-Command "vcpkg").Source
        $vcpkgDir = Split-Path -Parent $vcpkgPath
        # Set VCPKG_ROOT for future use
        $env:VCPKG_ROOT = $vcpkgDir
        return $vcpkgDir
    }
    
    return $null
}

# Check for prerequisites
Write-Host "Checking prerequisites..." -ForegroundColor Cyan

# Check for CMake
if (-not (Test-CommandExists "cmake")) {
    Write-Host "CMake is not installed. Please install CMake from https://cmake.org/download/" -ForegroundColor Red
    exit 1
} else {
    Write-Host "CMake is installed." -ForegroundColor Green
}

# Find vcpkg
$vcpkgPath = Find-VcpkgPath
if ($null -eq $vcpkgPath) {
    Write-Host "VCPKG_ROOT environment variable is not set or vcpkg is not found at the specified location." -ForegroundColor Red
    Write-Host "Please set VCPKG_ROOT to point to your vcpkg installation directory." -ForegroundColor Yellow
    Write-Host "Example: $env:VCPKG_ROOT = 'C:\path\to\vcpkg'" -ForegroundColor Yellow
    exit 1
} else {
    Write-Host "Found vcpkg at: $vcpkgPath" -ForegroundColor Green
    Write-Host "Using VCPKG_ROOT: $env:VCPKG_ROOT" -ForegroundColor Green
    
    # Add vcpkg to PATH for current session if not already there
    if (-not (Test-CommandExists "vcpkg")) {
        $env:PATH = "$vcpkgPath;$env:PATH"
    }
}

# Install required packages via vcpkg if not already installed
$packages = @("curl:x64-windows-static", "openssl:x64-windows-static", "zlib:x64-windows-static", "nlohmann-json:x64-windows-static")

foreach ($package in $packages) {
    if (-not (Test-VcpkgPackageInstalled $package)) {
        Write-Host "Installing $package..." -ForegroundColor Yellow
        & vcpkg install $package
    } else {
        Write-Host "$package is already installed" -ForegroundColor Green
    }
}

# Create build directory if it doesn't exist
if (-not (Test-Path "build")) {
    Write-Host "Creating build directory..." -ForegroundColor Cyan
    New-Item -ItemType Directory -Path "build" | Out-Null
}

# Navigate to build directory
Push-Location build

# Configure with CMake
Write-Host "Configuring with CMake..." -ForegroundColor Cyan
& cmake .. -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows-static

# Build the project
Write-Host "Building the project..." -ForegroundColor Cyan
& cmake --build . --config Release

# Check if build was successful
if ($LASTEXITCODE -eq 0) {
    Write-Host "Build completed successfully!" -ForegroundColor Green
    Write-Host "Executable location: $(Get-Location)\Release\yt_dlp_updater.exe" -ForegroundColor Green
} else {
    Write-Host "Build failed with error code: $LASTEXITCODE" -ForegroundColor Red
}

# Return to original directory
Pop-Location 