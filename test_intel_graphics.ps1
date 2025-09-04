# Intel Graphics Compatibility Test Script
# This script simulates Intel graphics behavior for testing CEF workarounds

param(
    [switch]$TestMode = $false,
    [switch]$AllowOverride = $false,
    [switch]$ShowLogs = $true,
    [string]$ExecutablePath = "otclient.exe"
)

Write-Host "Intel Graphics CEF Test Script" -ForegroundColor Cyan
Write-Host "==============================" -ForegroundColor Cyan
Write-Host ""

if ($TestMode) {
    Write-Host "Enabling Intel graphics test mode..." -ForegroundColor Yellow
    $env:CEF_TEST_INTEL_GRAPHICS = "1"
} else {
    Write-Host "Normal mode (Intel detection via hardware)" -ForegroundColor Green
    $env:CEF_TEST_INTEL_GRAPHICS = "0"
}

if ($AllowOverride) {
    Write-Host "Allowing Intel graphics override..." -ForegroundColor Yellow
    $env:CEF_ALLOW_INTEL_OVERRIDE = "1"
} else {
    $env:CEF_ALLOW_INTEL_OVERRIDE = "0"
}

Write-Host ""
Write-Host "Environment Configuration:" -ForegroundColor White
Write-Host "  CEF_TEST_INTEL_GRAPHICS = $env:CEF_TEST_INTEL_GRAPHICS" -ForegroundColor Gray
Write-Host "  CEF_ALLOW_INTEL_OVERRIDE = $env:CEF_ALLOW_INTEL_OVERRIDE" -ForegroundColor Gray
Write-Host ""

if ($TestMode) {
    Write-Host "Expected log messages when Intel mode is active:" -ForegroundColor Green
    Write-Host "  [Windows] TEST MODE: Simulating Intel graphics behavior" -ForegroundColor Gray
    Write-Host "  [Windows] Intel graphics detected - applying compatibility workarounds" -ForegroundColor Gray
    Write-Host "  [Windows] Intel compatibility flags applied" -ForegroundColor Gray
    Write-Host "  [Windows] Command line flags: (should include Intel-specific flags)" -ForegroundColor Gray
} else {
    Write-Host "Expected behavior: Normal GPU detection and configuration" -ForegroundColor Green
}

Write-Host ""
Write-Host "Starting OTClient..." -ForegroundColor Cyan

if (Test-Path $ExecutablePath) {
    & $ExecutablePath
} else {
    Write-Host "Error: Could not find $ExecutablePath" -ForegroundColor Red
    Write-Host "Please run this script from the OTClient directory or specify the correct path." -ForegroundColor Red
}

Write-Host ""
Write-Host "Test completed. Check the logs for Intel-specific messages." -ForegroundColor Cyan