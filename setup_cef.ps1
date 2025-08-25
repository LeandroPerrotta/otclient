# OTClient CEF Setup Script for Windows
# This script downloads, extracts, and compiles CEF for OTClient on Windows

param(
    [switch]$Install,
    [switch]$Global,
    [switch]$User,
    [switch]$Help
)

# Stop on first error
$ErrorActionPreference = "Stop"

$CEF_VERSION = "139.0.26+g9d80e0d+chromium-139.0.7258.139"
$CEF_URL = "https://cef-builds.spotifycdn.com/cef_binary_${CEF_VERSION}_windows64_minimal.tar.bz2"
$CEF_ARCHIVE = "$env:TEMP\cef_binary_${CEF_VERSION}_windows64_minimal.tar.bz2"

# Default to local development mode
$INSTALL_MODE = "local"
$CEF_RUNTIME_DIR = "$env:LOCALAPPDATA\otclient-cef"

# Parse command line arguments
if ($Help) {
    Write-Host "OTClient CEF Setup Script for Windows"
    Write-Host ""
    Write-Host "Usage: .\setup_cef.ps1 [OPTIONS]"
    Write-Host ""
    Write-Host "Options:"
    Write-Host "  (no option)     Install locally for development (./cef/)"
    Write-Host "  -Install        Install globally for all users (C:\Program Files\otclient-cef\)"
    Write-Host "  -User           Install for current user ($env:LOCALAPPDATA\otclient-cef\)"
    Write-Host "  -Help           Show this help message"
    Write-Host ""
    Write-Host "Examples:"
    Write-Host "  .\setup_cef.ps1              # Local development"
    Write-Host "  .\setup_cef.ps1 -Install     # Global installation (requires admin)"
    Write-Host "  .\setup_cef.ps1 -User        # User installation"
    exit 0
}

if ($Install -or $Global) {
    $INSTALL_MODE = "global"
    $CEF_RUNTIME_DIR = "C:\Program Files\otclient-cef"
}

if ($User) {
    $INSTALL_MODE = "user"
    $CEF_RUNTIME_DIR = "$env:LOCALAPPDATA\otclient-cef"
}

Write-Host "=== OTClient CEF Setup Script for Windows ==="
Write-Host "Version: ${CEF_VERSION}"
Write-Host "Install mode: ${INSTALL_MODE}"
Write-Host "Runtime directory: ${CEF_RUNTIME_DIR}"
Write-Host ""

# Check if CEF runtime is ready (check local cef/ directory first)
if ((Test-Path ".\cef") -and (Test-Path ".\cef\libcef.dll") -and (Test-Path ".\cef\libcef_dll_wrapper.lib")) {
    Write-Host "[OK] CEF runtime already exists and is ready"
    Write-Host "   Location: .\cef"
    Write-Host "   Libraries: .\cef\libcef.dll, .\cef\libcef_dll_wrapper.lib"
    Write-Host "[OK] Distribution files ready in ./cef/"
    exit 0
}

# Check for required tools
if (!(Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Host "[ERROR] CMake is required but not installed"
    Write-Host "   Please install CMake: https://cmake.org/download/"
    exit 1
}

# Check for Visual Studio
$VSWHERE = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $VSWHERE) {
    $VS_PATH = & $VSWHERE -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($VS_PATH) {
        $VCVARSALL = "$VS_PATH\VC\Auxiliary\Build\vcvarsall.bat"
        if (Test-Path $VCVARSALL) {
            Write-Host "[OK] Visual Studio found at: $VS_PATH"
        } else {
            Write-Host "[ERROR] Visual Studio C++ tools not found"
            Write-Host "   Please install Visual Studio with C++ development tools"
            exit 1
        }
    } else {
        Write-Host "[ERROR] Visual Studio with C++ tools not found"
        Write-Host "   Please install Visual Studio with C++ development tools"
        exit 1
    }
} else {
    Write-Host "[ERROR] Visual Studio not found"
    Write-Host "   Please install Visual Studio with C++ development tools"
    exit 1
}

# Check permissions for global installation
if ($INSTALL_MODE -eq "global") {
    $isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")
    if (!$isAdmin) {
        Write-Host "[ERROR] Global installation requires administrator privileges"
        Write-Host "   Please run PowerShell as Administrator"
        exit 1
    }
}

# Download CEF if not exists
if (!(Test-Path $CEF_ARCHIVE)) {
    Write-Host "[INFO] Downloading CEF to $env:TEMP (this may take a few minutes)..."
    try {
        Invoke-WebRequest -Uri $CEF_URL -OutFile $CEF_ARCHIVE -UseBasicParsing
        Write-Host "[OK] CEF downloaded successfully"
    } catch {
        Write-Host "[ERROR] Failed to download CEF: $($_.Exception.Message)"
        exit 1
    }
} else {
    Write-Host "[OK] CEF archive already exists in $env:TEMP"
}

# Extract CEF to runtime directory
Write-Host "[INFO] Extracting CEF to runtime directory..."
if (Test-Path $CEF_RUNTIME_DIR) {
    Write-Host "   Cleaning existing CEF directory..."
    Remove-Item $CEF_RUNTIME_DIR -Recurse -Force
}
New-Item -ItemType Directory -Path $CEF_RUNTIME_DIR -Force | Out-Null

# Use multiple extraction methods
$extracted = $false

# Method 1: Try 7-Zip first (best option)
$7ZIP_FOUND = $false
$7ZIP_PATH = ""

# Try to find 7-Zip in common locations
$7ZIP_PATHS = @(
    "7z",  # If in PATH
    "${env:ProgramFiles}\7-Zip\7z.exe",
    "${env:ProgramFiles(x86)}\7-Zip\7z.exe",
    "$env:LOCALAPPDATA\Programs\7-Zip\7z.exe"
)

foreach ($path in $7ZIP_PATHS) {
    if (Test-Path $path) {
        $7ZIP_PATH = $path
        $7ZIP_FOUND = $true
        Write-Host "   [INFO] Found 7-Zip at: $path"
        break
    }
}

if ($7ZIP_FOUND) {
    Write-Host "   Using 7-Zip to extract..."
    & $7ZIP_PATH x $CEF_ARCHIVE -o"$CEF_RUNTIME_DIR" -y | Out-Null
    if ($LASTEXITCODE -eq 0) {
        $extracted = $true
        Write-Host "   [OK] 7-Zip extraction successful"
    } else {
        Write-Host "   [WARN] 7-Zip extraction failed with exit code: $LASTEXITCODE"
    }
} else {
    Write-Host "   [INFO] 7-Zip not found in common locations"
}

# Method 2: Try PowerShell Expand-Archive (for .zip files)
if (!$extracted -and $CEF_ARCHIVE -like "*.zip") {
    Write-Host "   Using PowerShell Expand-Archive..."
    try {
        Expand-Archive -Path $CEF_ARCHIVE -DestinationPath $CEF_RUNTIME_DIR -Force
        $extracted = $true
        Write-Host "   [OK] PowerShell extraction successful"
    } catch {
        Write-Host "   [WARN] PowerShell extraction failed: $($_.Exception.Message)"
    }
}

# Method 3: Try tar with different options
if (!$extracted -and (Get-Command tar -ErrorAction SilentlyContinue)) {
    Write-Host "   Using tar to extract..."
    try {
        # Try tar with automatic decompression
        & tar -xaf $CEF_ARCHIVE -C $CEF_RUNTIME_DIR
        if ($LASTEXITCODE -eq 0) {
            $extracted = $true
            Write-Host "   [OK] tar extraction successful"
        }
    } catch {
        Write-Host "   [WARN] tar extraction failed: $($_.Exception.Message)"
    }
}

# Method 4: Try WinRAR if available
if (!$extracted -and (Get-Command winrar -ErrorAction SilentlyContinue)) {
    Write-Host "   Using WinRAR to extract..."
    & winrar x $CEF_ARCHIVE $CEF_RUNTIME_DIR\ | Out-Null
    if ($LASTEXITCODE -eq 0) {
        $extracted = $true
        Write-Host "   [OK] WinRAR extraction successful"
    }
}

# Method 5: Try to download as .zip instead
if (!$extracted) {
    Write-Host "   [INFO] Trying to download as .zip format..."
    $CEF_URL_ZIP = $CEF_URL -replace "\.tar\.bz2$", ".zip"
    $CEF_ARCHIVE_ZIP = $CEF_ARCHIVE -replace "\.tar\.bz2$", ".zip"
    
    try {
        Invoke-WebRequest -Uri $CEF_URL_ZIP -OutFile $CEF_ARCHIVE_ZIP -UseBasicParsing
        if (Test-Path $CEF_ARCHIVE_ZIP) {
            Write-Host "   [OK] Downloaded .zip version"
            Expand-Archive -Path $CEF_ARCHIVE_ZIP -DestinationPath $CEF_RUNTIME_DIR -Force
            $extracted = $true
            Write-Host "   [OK] .zip extraction successful"
            Remove-Item $CEF_ARCHIVE_ZIP -Force
        }
    } catch {
        Write-Host "   [WARN] .zip download failed: $($_.Exception.Message)"
    }
}

if (!$extracted) {
    Write-Host "[ERROR] Failed to extract CEF with any available method"
    Write-Host "   Please install one of the following:"
    Write-Host "   - 7-Zip: https://7-zip.org/"
    Write-Host "   - WinRAR: https://www.win-rar.com/"
    Write-Host "   - Or ensure tar supports bzip2 compression"
    exit 1
}

# Find the extracted directory or tar file
Write-Host "[DEBUG] Looking for extracted CEF directory or tar file..."
$CEF_EXTRACTED_DIR = Get-ChildItem -Path $CEF_RUNTIME_DIR -Directory -Name "cef_binary_*" | Select-Object -First 1
$CEF_TAR_FILE = Get-ChildItem -Path $CEF_RUNTIME_DIR -File -Name "cef_binary_*.tar" | Select-Object -First 1

if ($CEF_EXTRACTED_DIR) {
    Write-Host "[DEBUG] Found extracted directory: $CEF_EXTRACTED_DIR"
} elseif ($CEF_TAR_FILE) {
    Write-Host "[DEBUG] Found tar file: $CEF_TAR_FILE"
    Write-Host "[INFO] Extracting tar file..."
    $tarPath = Join-Path $CEF_RUNTIME_DIR $CEF_TAR_FILE
    
    # Extract the tar file
    if ($7ZIP_FOUND) {
        & $7ZIP_PATH x $tarPath -o"$CEF_RUNTIME_DIR" -y | Out-Null
        if ($LASTEXITCODE -eq 0) {
            Write-Host "[OK] Tar file extracted successfully"
            # Remove the tar file
            Remove-Item $tarPath -Force
            # Look for the extracted directory again
            $CEF_EXTRACTED_DIR = Get-ChildItem -Path $CEF_RUNTIME_DIR -Directory -Name "cef_binary_*" | Select-Object -First 1
            if ($CEF_EXTRACTED_DIR) {
                Write-Host "[DEBUG] Found extracted directory after tar extraction: $CEF_EXTRACTED_DIR"
            }
        } else {
            Write-Host "[ERROR] Failed to extract tar file"
            exit 1
        }
    } else {
        Write-Host "[ERROR] No extraction tool available for tar file"
        exit 1
    }
}

if (!$CEF_EXTRACTED_DIR) {
    Write-Host "[ERROR] Failed to find extracted CEF directory"
    Write-Host "[DEBUG] Contents of $CEF_RUNTIME_DIR"
    Get-ChildItem -Path $CEF_RUNTIME_DIR | ForEach-Object { Write-Host "   $($_.Name)" }
    exit 1
}

# Move contents to runtime directory
Write-Host "[INFO] Moving CEF files to runtime directory..."
$extractedPath = Join-Path $CEF_RUNTIME_DIR $CEF_EXTRACTED_DIR
Get-ChildItem -Path $extractedPath | Move-Item -Destination $CEF_RUNTIME_DIR -Force
Remove-Item $extractedPath -Force

Write-Host "[INFO] Cleaning up download..."
Remove-Item $CEF_ARCHIVE -Force -ErrorAction SilentlyContinue
Write-Host "[OK] CEF extracted to runtime directory"

# Compile CEF wrapper if not exists (like Linux script)
if (!(Test-Path "$CEF_RUNTIME_DIR\Release\libcef_dll_wrapper.lib")) {
    Write-Host "[INFO] Compiling CEF wrapper library..."
    
    # Go to CEF directory (like Linux script)
    Push-Location $CEF_RUNTIME_DIR
    
    try {
        # Configure CEF with Visual Studio 2022 and v142 toolset
        Write-Host "   Configuring CEF with Visual Studio 2022 and v142 toolset..."
        Write-Host "   Using MD_DynamicRelease to match OTClient runtime configuration..."
        & cmake -DCMAKE_BUILD_TYPE=Release -G "Visual Studio 17 2022" -A x64 -T v142 -DCEF_RUNTIME_LIBRARY_FLAG="/MD" .
        
        if ($LASTEXITCODE -eq 0) {
            # Compile wrapper
            Write-Host "   Compiling wrapper..."
            & cmake --build . --target libcef_dll_wrapper --config Release
            
            if ($LASTEXITCODE -eq 0) {
                Write-Host "[OK] CEF wrapper compiled successfully"
                
                # Move wrapper to Release directory if it was created elsewhere (like Linux script)
                if (Test-Path "libcef_dll_wrapper\Release\libcef_dll_wrapper.lib") {
                    Write-Host "   Moving wrapper to Release directory..."
                    Copy-Item "libcef_dll_wrapper\Release\libcef_dll_wrapper.lib" "Release\" -Force
                    Write-Host "[OK] Wrapper moved to Release directory"
                }
            } else {
                Write-Host "[ERROR] Failed to compile CEF wrapper"
                exit 1
            }
        } else {
            Write-Host "[ERROR] Failed to configure CEF"
            exit 1
        }
    } finally {
        Pop-Location
    }
} else {
    Write-Host "[OK] CEF wrapper already exists"
}

# Verify all necessary files are in place
Write-Host "[INFO] Verifying CEF runtime files..."
if (!(Test-Path "$CEF_RUNTIME_DIR\Release")) {
    Write-Host "[ERROR] Release directory not found"
    exit 1
}

if (!(Test-Path "$CEF_RUNTIME_DIR\include")) {
    Write-Host "[ERROR] Include directory not found"
    exit 1
}

if (!(Test-Path "$CEF_RUNTIME_DIR\Resources")) {
    Write-Host "[ERROR] Resources directory not found"
    exit 1
}

Write-Host "[OK] CEF runtime files verified"

Write-Host "[OK] CEF runtime files copied to $CEF_RUNTIME_DIR"

# Copy essential files to local cef/ directory (runtime only, no build files)
Write-Host "[INFO] Copying essential runtime files to ./cef/ for distribution..."

# Clean and create local cef directory
if (Test-Path ".\cef") {
    Write-Host "   Cleaning existing ./cef directory..."
    Remove-Item ".\cef" -Recurse -Force
}
New-Item -ItemType Directory -Path ".\cef" -Force | Out-Null

# Copy essential runtime files from CEF runtime directory (like Linux script)
Write-Host "   Copying CEF runtime files..."
Copy-Item "$CEF_RUNTIME_DIR\Release\*" ".\cef\" -Force -ErrorAction SilentlyContinue
Copy-Item "$CEF_RUNTIME_DIR\Resources\*" ".\cef\" -Force -ErrorAction SilentlyContinue

# Copy locales directory if it exists (like Linux script)
if (Test-Path "$CEF_RUNTIME_DIR\Resources\locales") {
    Copy-Item "$CEF_RUNTIME_DIR\Resources\locales" ".\cef\" -Recurse -Force
    Write-Host "   [OK] Locales directory copied"
}

Write-Host "[OK] Essential runtime files ready in ./cef/ (no build artifacts)"

Write-Host ""
Write-Host "[SUCCESS] CEF setup completed successfully!"
Write-Host "   Install mode: ${INSTALL_MODE}"
Write-Host "   Runtime location: $CEF_RUNTIME_DIR"
Write-Host "   Distribution: ./cef/ (essential runtime files only)"
Write-Host ""
Write-Host "[INFO] You can now compile OTClient with CEF support:"
Write-Host "   mkdir build"
Write-Host "   cd build"
Write-Host "   cmake -DUSE_CEF=ON -G `"Visual Studio 17 2022`" -A x64 -T v142 -DCMAKE_TOOLCHAIN_FILE=`"`$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake`" -DVCPKG_TARGET_TRIPLET=x64-windows-v142 .."
Write-Host "   cmake --build . --config RelWithDebInfo --parallel"
Write-Host ""
if ($INSTALL_MODE -eq "global") {
    Write-Host "[INFO] Global installation complete! Other users can now compile OTClient with CEF."
} elseif ($INSTALL_MODE -eq "user") {
    Write-Host "[INFO] User installation complete! CEF is available for your user account."
} else {
    Write-Host "[INFO] Local development setup complete! CEF is ready for development."
}
Write-Host "[INFO] Distribution files are ready in ./cef/ (no build artifacts)"
