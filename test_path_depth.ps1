# Script para testar o problema de profundidade de diretório do CEF
# Este script cria builds de teste em diferentes profundidades

param(
    [string]$BuildDir = "build",
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

Write-Host "=== CEF Path Depth Test ===" -ForegroundColor Green

# Verificar se build existe
if (-not (Test-Path "$BuildDir\otclient.exe")) {
    Write-Error "Build not found at $BuildDir\otclient.exe"
    exit 1
}

# Criar diretórios de teste com diferentes profundidades
$TestDirs = @(
    "C:\TestCEF1",                           # Profundidade 1
    "C:\TestCEF2\Level2",                    # Profundidade 2  
    "C:\TestCEF3\Level2\Level3",             # Profundidade 3
    "C:\TestCEF4\Level2\Level3\Level4",      # Profundidade 4
    "C:\TestCEF5\Level2\Level3\Level4\Level5" # Profundidade 5
)

foreach ($testDir in $TestDirs) {
    Write-Host ""
    Write-Host "Testing directory: $testDir" -ForegroundColor Cyan
    
    # Calcular profundidade
    $depth = ($testDir -split '\\').Count - 1
    Write-Host "Directory depth: $depth levels"
    
    # Limpar se existe
    if (Test-Path $testDir) {
        Remove-Item -Recurse -Force $testDir
    }
    
    # Criar diretório
    New-Item -ItemType Directory -Path $testDir -Force | Out-Null
    
    # Copiar arquivos essenciais
    Copy-Item "$BuildDir\otclient.exe" $testDir
    Copy-Item "$BuildDir\otclient_cef_subproc.exe" $testDir -ErrorAction SilentlyContinue
    Copy-Item -Recurse "data" $testDir -ErrorAction SilentlyContinue
    Copy-Item -Recurse "modules" $testDir -ErrorAction SilentlyContinue
    Copy-Item -Recurse "webviews" $testDir -ErrorAction SilentlyContinue
    Copy-Item -Recurse "cef" $testDir -ErrorAction SilentlyContinue
    Copy-Item "init.lua" $testDir -ErrorAction SilentlyContinue
    Copy-Item "otclientrc.lua" $testDir -ErrorAction SilentlyContinue
    
    Write-Host "✓ Files copied to $testDir"
    
    # Criar script de teste
    $testScript = @"
@echo off
echo Testing CEF at depth $depth
echo Directory: $testDir
echo.
echo Starting OTClient...
cd /d "$testDir"
otclient.exe
pause
"@
    
    $testScript | Out-File -FilePath "$testDir\test_cef.bat" -Encoding ASCII
    
    Write-Host "✓ Test script created: $testDir\test_cef.bat"
}

Write-Host ""
Write-Host "=== Test Setup Complete ===" -ForegroundColor Green
Write-Host "Manual test procedure:"
Write-Host "1. Run each test_cef.bat script"
Write-Host "2. Try to load a webview in each"
Write-Host "3. Check if OnAcceleratedPaint is called"
Write-Host "4. Note at which depth it stops working"
Write-Host ""
Write-Host "Expected results based on your findings:"
Write-Host "✅ C:\TestCEF1 (depth 1) - Should work"
Write-Host "❌ C:\TestCEF2\Level2 (depth 2) - May not work"
Write-Host "❌ Deeper levels - Likely won't work"