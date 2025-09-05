#!/bin/bash
# Script para criar uma build portável do OTClient com CEF
# Este script copia todos os arquivos necessários para uma pasta de distribuição

set -e

BUILD_DIR="${1:-build}"
OUTPUT_DIR="${2:-otclient-portable}"
CONFIGURATION="${3:-Release}"

echo "=== OTClient Portable Build Creator ==="
echo "Build Directory: $BUILD_DIR"
echo "Output Directory: $OUTPUT_DIR"
echo "Configuration: $CONFIGURATION"
echo

# Verificar se o diretório de build existe
if [ ! -d "$BUILD_DIR" ]; then
    echo "ERROR: Build directory '$BUILD_DIR' not found. Please compile the project first."
    exit 1
fi

# Criar diretório de saída
if [ -d "$OUTPUT_DIR" ]; then
    echo "Removing existing output directory..."
    rm -rf "$OUTPUT_DIR"
fi
mkdir -p "$OUTPUT_DIR"

echo "Creating portable build..."

# Copiar executáveis principais
MAIN_EXE="$BUILD_DIR/otclient"
SUBPROC_EXE="$BUILD_DIR/otclient_cef_subproc"

if [ -f "$MAIN_EXE" ]; then
    cp "$MAIN_EXE" "$OUTPUT_DIR/"
    chmod +x "$OUTPUT_DIR/otclient"
    echo "✓ Copied otclient"
else
    echo "ERROR: Main executable not found at $MAIN_EXE"
    exit 1
fi

if [ -f "$SUBPROC_EXE" ]; then
    cp "$SUBPROC_EXE" "$OUTPUT_DIR/"
    chmod +x "$OUTPUT_DIR/otclient_cef_subproc"
    echo "✓ Copied otclient_cef_subproc"
else
    echo "WARNING: CEF subprocess executable not found at $SUBPROC_EXE"
fi

# Copiar arquivos de dados do projeto
DATA_DIRS=("data" "modules" "mods")
for dir in "${DATA_DIRS[@]}"; do
    if [ -d "$dir" ]; then
        cp -r "$dir" "$OUTPUT_DIR/"
        echo "✓ Copied $dir/"
    else
        echo "WARNING: Directory $dir not found"
    fi
done

# Copiar arquivos de configuração
CONFIG_FILES=("init.lua" "otclientrc.lua")
for file in "${CONFIG_FILES[@]}"; do
    if [ -f "$file" ]; then
        cp "$file" "$OUTPUT_DIR/"
        echo "✓ Copied $file"
    fi
done

# Criar diretório CEF e copiar arquivos necessários
CEF_OUTPUT_DIR="$OUTPUT_DIR/cef"
mkdir -p "$CEF_OUTPUT_DIR"

# Procurar pelo diretório CEF local
LOCAL_CEF_DIR="./cef"
if [ -d "$LOCAL_CEF_DIR" ]; then
    echo "Found local CEF directory, copying files..."
    
    # Arquivos essenciais do CEF para Linux
    CEF_FILES=(
        "libcef.so"
        "libcef_dll_wrapper.a"
        "libEGL.so"
        "libGLESv2.so"
        "swiftshader/libEGL.so"
        "swiftshader/libGLESv2.so"
    )
    
    for file in "${CEF_FILES[@]}"; do
        SOURCE_PATH="$LOCAL_CEF_DIR/$file"
        if [ -f "$SOURCE_PATH" ]; then
            # Criar diretório se necessário (para swiftshader)
            DEST_DIR="$CEF_OUTPUT_DIR/$(dirname "$file")"
            mkdir -p "$DEST_DIR"
            cp "$SOURCE_PATH" "$CEF_OUTPUT_DIR/$file"
            echo "  ✓ Copied CEF file: $file"
        else
            echo "  WARNING: CEF file not found: $file"
        fi
    done
    
    # Copiar diretório locales
    LOCALES_SOURCE="$LOCAL_CEF_DIR/locales"
    if [ -d "$LOCALES_SOURCE" ]; then
        cp -r "$LOCALES_SOURCE" "$CEF_OUTPUT_DIR/"
        echo "  ✓ Copied locales directory"
    fi
    
    # Copiar outros arquivos importantes
    OTHER_CEF_FILES=("icudtl.dat" "snapshot_blob.bin" "v8_context_snapshot.bin")
    for file in "${OTHER_CEF_FILES[@]}"; do
        SOURCE_PATH="$LOCAL_CEF_DIR/$file"
        if [ -f "$SOURCE_PATH" ]; then
            cp "$SOURCE_PATH" "$CEF_OUTPUT_DIR/"
            echo "  ✓ Copied CEF resource: $file"
        fi
    done
    
    # Copiar subprocess se estiver no diretório CEF
    CEF_SUBPROC="$LOCAL_CEF_DIR/otclient_cef_subproc"
    if [ -f "$CEF_SUBPROC" ]; then
        cp "$CEF_SUBPROC" "$CEF_OUTPUT_DIR/"
        chmod +x "$CEF_OUTPUT_DIR/otclient_cef_subproc"
        echo "  ✓ Copied CEF subprocess from cef directory"
    fi
    
else
    echo "WARNING: Local CEF directory not found at $LOCAL_CEF_DIR"
    echo "The portable build may not work without CEF files"
fi

# Criar diretório cache vazio
CACHE_DIR="$CEF_OUTPUT_DIR/cache"
mkdir -p "$CACHE_DIR"

# Criar arquivo README para a build portável
README_PATH="$OUTPUT_DIR/README_PORTABLE.txt"
cat > "$README_PATH" << 'EOF'
OTClient Portable Build
======================

This is a portable build of OTClient with CEF (Chromium Embedded Framework) support.

Files and Directories:
- otclient: Main executable
- otclient_cef_subproc: CEF subprocess executable
- cef/: CEF runtime files and libraries
- data/: Game data files
- modules/: Game modules
- mods/: Game modifications
- init.lua, otclientrc.lua: Configuration files

Usage:
1. Extract this folder anywhere on your system
2. Run ./otclient
3. The CEF browser functionality should work regardless of the folder location

The build is configured to find all CEF files relative to the executable location,
making it fully portable.

Note for Linux:
- Make sure the executable has execute permissions: chmod +x otclient
- You may need to install system dependencies like X11 libraries

EOF
echo "Build created on: $(date)" >> "$README_PATH"
echo "✓ Created README_PORTABLE.txt"

# Criar script de execução
LAUNCH_SCRIPT="$OUTPUT_DIR/run_otclient.sh"
cat > "$LAUNCH_SCRIPT" << 'EOF'
#!/bin/bash
# OTClient Launcher Script
# This script ensures proper library paths and launches OTClient

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Set library path to include CEF directory
export LD_LIBRARY_PATH="$SCRIPT_DIR/cef:$LD_LIBRARY_PATH"

# Change to the script directory
cd "$SCRIPT_DIR"

# Launch OTClient
echo "Starting OTClient..."
./otclient "$@"
EOF
chmod +x "$LAUNCH_SCRIPT"
echo "✓ Created run_otclient.sh launcher script"

echo
echo "=== Portable Build Complete ==="
echo "Output directory: $OUTPUT_DIR"
echo
echo "To test the portable build:"
echo "1. Move the '$OUTPUT_DIR' folder to a different location"
echo "2. Run ./run_otclient.sh from the new location"
echo "3. Test CEF functionality (webview modules)"
echo

# Mostrar tamanho da build
BUILD_SIZE=$(du -sh "$OUTPUT_DIR" | cut -f1)
echo "Build size: $BUILD_SIZE"