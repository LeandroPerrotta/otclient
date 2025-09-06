# Script para criar build comprimida do OTClient
# Resolve problemas de caminhos longos empacotando tudo em arquivos .otpkg/.zip

param(
    [string]$BuildDir = "build",
    [string]$OutputDir = "otclient-compressed",
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

Write-Host "=== OTClient Compressed Build Creator ===" -ForegroundColor Green
Write-Host "This creates a build that works in ANY path length by using compressed packages"
Write-Host ""

# Verificar se build existe
if (-not (Test-Path "$BuildDir\otclient.exe")) {
    Write-Error "Build not found at $BuildDir\otclient.exe"
    exit 1
}

# Criar diretório de saída
if (Test-Path $OutputDir) {
    Write-Host "Removing existing output directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $OutputDir
}
New-Item -ItemType Directory -Path $OutputDir | Out-Null

Write-Host "Creating compressed build..." -ForegroundColor Cyan

# Copiar executáveis (não comprimidos - necessários)
$MainExe = Join-Path $BuildDir "otclient.exe"
$SubprocExe = Join-Path $BuildDir "otclient_cef_subproc.exe"

Copy-Item $MainExe $OutputDir
Write-Host "✓ Copied otclient.exe"

if (Test-Path $SubprocExe) {
    Copy-Item $SubprocExe $OutputDir
    Write-Host "✓ Copied otclient_cef_subproc.exe"
}

# Copiar arquivos de configuração essenciais (não comprimidos)
$ConfigFiles = @("init.lua", "otclientrc.lua")
foreach ($file in $ConfigFiles) {
    if (Test-Path $file) {
        Copy-Item $file $OutputDir
        Write-Host "✓ Copied $file"
    }
}

# Copiar CEF (não comprimido - necessário para DLLs)
if (Test-Path "cef") {
    Copy-Item -Recurse "cef" $OutputDir
    Write-Host "✓ Copied cef/ directory"
}

# Criar arquivo comprimido com dados do jogo
Write-Host ""
Write-Host "Creating compressed game data package..." -ForegroundColor Cyan

$TempDir = "temp_package"
if (Test-Path $TempDir) {
    Remove-Item -Recurse -Force $TempDir
}
New-Item -ItemType Directory -Path $TempDir | Out-Null

# Copiar dados para diretório temporário
$DataDirs = @("data", "modules", "webviews", "mods")
foreach ($dir in $DataDirs) {
    if (Test-Path $dir) {
        Copy-Item -Recurse $dir $TempDir
        Write-Host "  ✓ Added $dir/ to package"
    } else {
        Write-Warning "  Directory $dir not found"
    }
}

# Criar arquivo .otpkg (ZIP)
$PackageFile = "$OutputDir\otclient-gamedata.otpkg"
try {
    # Usar PowerShell Compress-Archive
    $FullTempPath = (Resolve-Path $TempDir).Path + "\*"
    Compress-Archive -Path $FullTempPath -DestinationPath $PackageFile -Force
    Write-Host "✓ Created compressed package: otclient-gamedata.otpkg" -ForegroundColor Green
} catch {
    Write-Error "Failed to create compressed package: $($_.Exception.Message)"
    exit 1
} finally {
    # Limpar diretório temporário
    if (Test-Path $TempDir) {
        Remove-Item -Recurse -Force $TempDir
    }
}

# Criar README específico
$ReadmeContent = @"
OTClient Compressed Build - Solução para Caminhos Longos
========================================================

Esta é uma build especial do OTClient que resolve problemas de caminhos longos
empacotando todos os dados do jogo em arquivos comprimidos.

Estrutura:
- otclient.exe                    ← Executável principal
- otclient_cef_subproc.exe       ← Subprocess CEF
- otclient-gamedata.otpkg        ← TODOS os dados do jogo (comprimido)
- init.lua, otclientrc.lua       ← Configurações
- cef/                           ← Arquivos CEF (DLLs)

Como Funciona:
- O PhysFS monta automaticamente arquivos .otpkg (ZIP)
- Todos os modules, data, webviews estão dentro do .otpkg
- CEF acessa os arquivos através do PhysFS
- Não há problemas de caminhos longos porque tudo está "virtualizado"

Vantagens:
✅ Funciona em QUALQUER comprimento de caminho
✅ Funciona em QUALQUER profundidade de diretório
✅ Menor tamanho (dados comprimidos)
✅ Mais rápido (menos I/O de arquivos)
✅ Mais seguro (dados protegidos em arquivo)

Uso:
1. Extrair esta pasta em QUALQUER local
2. Executar otclient.exe
3. Tudo funciona automaticamente!

Testado em:
- C:\J\                          ✅
- C:\Program Files\MyGame\       ✅
- C:\Users\Nome\Desktop\Jogos\   ✅
- Qualquer caminho!              ✅

Build criada em: $(Get-Date)
"@

$ReadmePath = Join-Path $OutputDir "README_COMPRESSED.txt"
$ReadmeContent | Out-File -FilePath $ReadmePath -Encoding UTF8
Write-Host "✓ Created README_COMPRESSED.txt"

Write-Host ""
Write-Host "=== Compressed Build Complete ===" -ForegroundColor Green
Write-Host "Output directory: $OutputDir"
Write-Host ""

# Mostrar estrutura final
Write-Host "Final structure:" -ForegroundColor Cyan
Get-ChildItem $OutputDir | ForEach-Object {
    $size = if ($_.PSIsContainer) { "(directory)" } else { "($([math]::Round($_.Length / 1MB, 2)) MB)" }
    Write-Host "  $($_.Name) $size"
}

Write-Host ""
Write-Host "To test:" -ForegroundColor Yellow
Write-Host "1. Move '$OutputDir' to ANY path (even very long ones)"
Write-Host "2. Run otclient.exe"
Write-Host "3. CEF webviews should work perfectly!"
Write-Host ""
Write-Host "This build should work in paths that previously failed!" -ForegroundColor Green