# Script para limpar arquivos .otpkg antigos e testar build comprimida

param(
    [string]$TestDir = "C:\Game"
)

$ErrorActionPreference = "Stop"

Write-Host "=== OTClient Compressed Build Cleaner & Tester ===" -ForegroundColor Green

# Limpar arquivos .otpkg existentes
Write-Host "Cleaning existing .otpkg files..." -ForegroundColor Yellow
Get-ChildItem -Path $TestDir -Filter "*.otpkg" -ErrorAction SilentlyContinue | ForEach-Object {
    Write-Host "  Removing: $($_.Name)"
    Remove-Item $_.FullName -Force
}

# Limpar diretórios de dados antigos (se existirem)
$DataDirs = @("data", "modules", "webviews", "mods")
foreach ($dir in $DataDirs) {
    $fullPath = Join-Path $TestDir $dir
    if (Test-Path $fullPath) {
        Write-Host "  Removing directory: $dir"
        Remove-Item -Recurse -Force $fullPath
    }
}

Write-Host "✓ Cleanup complete" -ForegroundColor Green
Write-Host ""

# Criar build comprimida limpa
Write-Host "Creating fresh compressed build..." -ForegroundColor Cyan
& .\create_compressed_build.ps1 -OutputDir $TestDir

Write-Host ""
Write-Host "=== Test Instructions ===" -ForegroundColor Yellow
Write-Host "1. Navigate to: $TestDir"
Write-Host "2. Run: otclient.exe"
Write-Host "3. Check logs for package mounting messages"
Write-Host "4. Test CEF webviews"
Write-Host ""
Write-Host "Expected structure in $TestDir:" -ForegroundColor Cyan
Write-Host "  otclient.exe"
Write-Host "  gamedata.otpkg          ← Single compressed file"
Write-Host "  init.lua"
Write-Host "  cef/"
Write-Host ""
Write-Host "If you see 'filename is illegal' errors, check the logs for details." -ForegroundColor Yellow