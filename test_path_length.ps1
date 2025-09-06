# Script para testar thresholds específicos de comprimento de caminho
# Baseado na descoberta de que o problema é comprimento, não profundidade

param(
    [string]$BuildDir = "build",
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

Write-Host "=== CEF Path Length Threshold Test ===" -ForegroundColor Green

# Verificar se build existe
if (-not (Test-Path "$BuildDir\otclient.exe")) {
    Write-Error "Build not found at $BuildDir\otclient.exe"
    exit 1
}

# Função para criar string de tamanho específico
function Create-PathString($length, $prefix = "C:\") {
    $remaining = $length - $prefix.Length
    if ($remaining -le 0) { return $prefix }
    
    $segments = @()
    while ($remaining -gt 0) {
        if ($remaining -gt 10) {
            $segments += "A" * 8  # 8 chars + \ = 9
            $remaining -= 9
        } else {
            $segments += "A" * ($remaining - 1)  # -1 for the final \
            $remaining = 0
        }
    }
    
    return $prefix + ($segments -join "\")
}

# Definir thresholds de teste baseados no padrão observado
$TestCases = @(
    @{ Length = 20; Expected = "✅ Should work 100%" },
    @{ Length = 30; Expected = "✅ Should work 100%" },
    @{ Length = 40; Expected = "⚠️ May work 70%" },
    @{ Length = 50; Expected = "⚠️ May work 50%" },
    @{ Length = 60; Expected = "❌ Likely fails" },
    @{ Length = 80; Expected = "❌ Should fail 100%" },
    @{ Length = 100; Expected = "❌ Definitely fails" }
)

Write-Host ""
Write-Host "Creating test directories with specific path lengths..." -ForegroundColor Cyan

foreach ($testCase in $TestCases) {
    $length = $testCase.Length
    $expected = $testCase.Expected
    
    # Criar caminho do tamanho exato
    $testPath = Create-PathString $length
    
    # Ajustar se necessário
    if ($testPath.Length -ne $length) {
        $diff = $length - $testPath.Length
        if ($diff -gt 0) {
            $testPath = $testPath.TrimEnd('\') + ("X" * $diff)
        } else {
            $testPath = $testPath.Substring(0, $length)
        }
    }
    
    Write-Host ""
    Write-Host "Test Case: $length characters" -ForegroundColor Yellow
    Write-Host "Path: $testPath" -ForegroundColor Gray
    Write-Host "Actual Length: $($testPath.Length) characters"
    Write-Host "Expected: $expected"
    
    try {
        # Limpar se existe
        if (Test-Path $testPath) {
            Remove-Item -Recurse -Force $testPath
        }
        
        # Criar diretório
        New-Item -ItemType Directory -Path $testPath -Force | Out-Null
        
        # Copiar arquivos essenciais
        Copy-Item "$BuildDir\otclient.exe" $testPath
        Copy-Item "$BuildDir\otclient_cef_subproc.exe" $testPath -ErrorAction SilentlyContinue
        Copy-Item -Recurse "data" $testPath -ErrorAction SilentlyContinue
        Copy-Item -Recurse "modules" $testPath -ErrorAction SilentlyContinue
        Copy-Item -Recurse "webviews" $testPath -ErrorAction SilentlyContinue
        Copy-Item -Recurse "cef" $testPath -ErrorAction SilentlyContinue
        Copy-Item "init.lua" $testPath -ErrorAction SilentlyContinue
        Copy-Item "otclientrc.lua" $testPath -ErrorAction SilentlyContinue
        
        # Criar script de teste
        $testScript = @"
@echo off
echo Testing CEF with path length: $length characters
echo Path: $testPath
echo Expected: $expected
echo.
echo Starting OTClient...
echo Look for PATH LENGTH ANALYSIS in cef.log
echo.
cd /d "$testPath"
otclient.exe
pause
"@
        
        $testScript | Out-File -FilePath "$testPath\test_length_$length.bat" -Encoding ASCII
        
        Write-Host "✓ Created test at: $testPath\test_length_$length.bat" -ForegroundColor Green
        
    } catch {
        Write-Host "❌ Failed to create test for length $length : $($_.Exception.Message)" -ForegroundColor Red
    }
}

Write-Host ""
Write-Host "=== Test Setup Complete ===" -ForegroundColor Green
Write-Host ""
Write-Host "Manual test procedure:"
Write-Host "1. Run each test_length_*.bat script in order (shortest to longest)"
Write-Host "2. Try to load a webview in each"
Write-Host "3. Check cef.log for 'PATH LENGTH ANALYSIS'"
Write-Host "4. Note the exact length where OnAcceleratedPaint stops being called"
Write-Host "5. Look for the workaround message: 'Path too long, implementing workaround'"
Write-Host ""
Write-Host "This will help us find the exact threshold and validate the workaround!"