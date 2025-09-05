# Script para criar uma build portável do OTClient com CEF
# Este script copia todos os arquivos necessários para uma pasta de distribuição

param(
    [string]$BuildDir = "build",
    [string]$OutputDir = "otclient-portable",
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

Write-Host "=== OTClient Portable Build Creator ===" -ForegroundColor Green
Write-Host "Build Directory: $BuildDir"
Write-Host "Output Directory: $OutputDir"
Write-Host "Configuration: $Configuration"
Write-Host ""

# Verificar se o diretório de build existe
if (-not (Test-Path $BuildDir)) {
    Write-Error "Build directory '$BuildDir' not found. Please compile the project first."
    exit 1
}

# Criar diretório de saída
if (Test-Path $OutputDir) {
    Write-Host "Removing existing output directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $OutputDir
}
New-Item -ItemType Directory -Path $OutputDir | Out-Null

Write-Host "Creating portable build..." -ForegroundColor Cyan

# Copiar executáveis principais
$MainExe = Join-Path $BuildDir "otclient.exe"
$SubprocExe = Join-Path $BuildDir "otclient_cef_subproc.exe"

if (Test-Path $MainExe) {
    Copy-Item $MainExe $OutputDir
    Write-Host "✓ Copied otclient.exe"
} else {
    Write-Error "Main executable not found at $MainExe"
}

if (Test-Path $SubprocExe) {
    Copy-Item $SubprocExe $OutputDir
    Write-Host "✓ Copied otclient_cef_subproc.exe"
} else {
    Write-Warning "CEF subprocess executable not found at $SubprocExe"
}

# Copiar arquivos de dados do projeto
$DataDirs = @("data", "modules", "mods")
foreach ($dir in $DataDirs) {
    if (Test-Path $dir) {
        Copy-Item -Recurse $dir $OutputDir
        Write-Host "✓ Copied $dir/"
    } else {
        Write-Warning "Directory $dir not found"
    }
}

# Copiar arquivos de configuração
$ConfigFiles = @("init.lua", "otclientrc.lua")
foreach ($file in $ConfigFiles) {
    if (Test-Path $file) {
        Copy-Item $file $OutputDir
        Write-Host "✓ Copied $file"
    }
}

# Criar diretório CEF e copiar arquivos necessários
$CefOutputDir = Join-Path $OutputDir "cef"
New-Item -ItemType Directory -Path $CefOutputDir | Out-Null

# Procurar pelo diretório CEF local
$LocalCefDir = ".\cef"
if (Test-Path $LocalCefDir) {
    Write-Host "Found local CEF directory, copying files..." -ForegroundColor Cyan
    
    # Arquivos essenciais do CEF para Windows
    $CefFiles = @(
        "libcef.dll",
        "libcef_dll_wrapper.lib",
        "chrome_elf.dll",
        "d3dcompiler_47.dll",
        "libEGL.dll",
        "libGLESv2.dll",
        "vk_swiftshader.dll",
        "vulkan-1.dll"
    )
    
    foreach ($file in $CefFiles) {
        $sourcePath = Join-Path $LocalCefDir $file
        if (Test-Path $sourcePath) {
            Copy-Item $sourcePath $CefOutputDir
            Write-Host "  ✓ Copied CEF file: $file"
        } else {
            Write-Warning "  CEF file not found: $file"
        }
    }
    
    # Copiar diretório locales
    $LocalesSource = Join-Path $LocalCefDir "locales"
    if (Test-Path $LocalesSource) {
        Copy-Item -Recurse $LocalesSource $CefOutputDir
        Write-Host "  ✓ Copied locales directory"
    }
    
    # Copiar outros arquivos importantes
    $OtherCefFiles = @("icudtl.dat", "snapshot_blob.bin", "v8_context_snapshot.bin")
    foreach ($file in $OtherCefFiles) {
        $sourcePath = Join-Path $LocalCefDir $file
        if (Test-Path $sourcePath) {
            Copy-Item $sourcePath $CefOutputDir
            Write-Host "  ✓ Copied CEF resource: $file"
        }
    }
    
    # Copiar subprocess se estiver no diretório CEF
    $CefSubproc = Join-Path $LocalCefDir "otclient_cef_subproc.exe"
    if (Test-Path $CefSubproc) {
        Copy-Item $CefSubproc $CefOutputDir
        Write-Host "  ✓ Copied CEF subprocess from cef directory"
    }
    
} else {
    Write-Warning "Local CEF directory not found at $LocalCefDir"
    Write-Warning "The portable build may not work without CEF files"
}

# Criar diretório cache vazio
$CacheDir = Join-Path $CefOutputDir "cache"
New-Item -ItemType Directory -Path $CacheDir | Out-Null

# Criar arquivo README para a build portável
$ReadmeContent = @"
OTClient Portable Build
======================

This is a portable build of OTClient with CEF (Chromium Embedded Framework) support.

Files and Directories:
- otclient.exe: Main executable
- otclient_cef_subproc.exe: CEF subprocess executable
- cef/: CEF runtime files and libraries
- data/: Game data files
- modules/: Game modules
- mods/: Game modifications
- init.lua, otclientrc.lua: Configuration files

Usage:
1. Extract this folder anywhere on your system
2. Run otclient.exe
3. The CEF browser functionality should work regardless of the folder location

The build is configured to find all CEF files relative to the executable location,
making it fully portable.

Build created on: $(Get-Date)
"@

$ReadmePath = Join-Path $OutputDir "README_PORTABLE.txt"
$ReadmeContent | Out-File -FilePath $ReadmePath -Encoding UTF8
Write-Host "✓ Created README_PORTABLE.txt"

Write-Host ""
Write-Host "=== Portable Build Complete ===" -ForegroundColor Green
Write-Host "Output directory: $OutputDir"
Write-Host ""
Write-Host "To test the portable build:"
Write-Host "1. Move the '$OutputDir' folder to a different location"
Write-Host "2. Run otclient.exe from the new location"
Write-Host "3. Test CEF functionality (webview modules)"
Write-Host ""

# Mostrar tamanho da build
$BuildSize = (Get-ChildItem -Recurse $OutputDir | Measure-Object -Property Length -Sum).Sum
$BuildSizeMB = [math]::Round($BuildSize / 1MB, 2)
Write-Host "Build size: $BuildSizeMB MB" -ForegroundColor Cyan