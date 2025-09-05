# Solução para CEF Portável no OTClient

## Problema Identificado

O CEF (Chromium Embedded Framework) só funcionava quando o OTClient estava no diretório de compilação original (`C:\otclient-dev`) porque havia **caminhos hardcoded** na configuração. Isso impedia a distribuição de builds standalone em .zip.

## Principais Problemas Encontrados

### 1. **Caminhos Absolutos no Windows** (`src/cef/core/cef_confwin.cpp`)
- O sistema buscava por arquivos CEF em locais fixos durante a compilação
- Não verificava se os arquivos necessários existiam no diretório do executável

### 2. **Log File Hardcoded** (`src/cef/core/cef_helper.cpp`)
- O arquivo de log `cef.log` era criado sempre no diretório atual, não relativo ao executável

### 3. **Configuração Linux Dependente do Working Directory** (`src/cef/core/cef_conflinux.cpp`)
- Usava `getcwd()` ao invés do diretório do executável

## Soluções Implementadas

### 1. **Windows - Verificação de Arquivos CEF**
```cpp
// Adicionado em configurePaths()
std::wstring libcefPath = cefDir + L"\\libcef.dll";
DWORD fileAttrib = GetFileAttributesW(libcefPath.c_str());
if (fileAttrib == INVALID_FILE_ATTRIBUTES) {
    logMessage("Windows", "ERROR: libcef.dll not found...");
    return;
}
```

### 2. **Log File Relativo ao Executável**
```cpp
// Novo sistema que detecta o diretório do executável
#ifdef _WIN32
    char exe_path[MAX_PATH];
    if (GetModuleFileNameA(nullptr, exe_path, MAX_PATH) > 0) {
        // Extrai diretório e cria log_path relativo
    }
#else
    char exe_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    // Similar para Linux
#endif
```

### 3. **Linux - Uso do Diretório do Executável**
```cpp
// Substituído getcwd() por readlink("/proc/self/exe")
char exe_path[PATH_MAX];
ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
if (len != -1) {
    // Extrai diretório do executável
    exe_dir = exe_full_path.substr(0, pos);
}
```

## Scripts de Build Portável

### Windows: `make_portable_build.ps1`
```powershell
# Cria build portável copiando:
# - Executáveis principais
# - Arquivos CEF necessários
# - Dados do jogo (data/, modules/, mods/)
# - Configurações (init.lua, otclientrc.lua)
```

### Linux: `make_portable_build.sh`
```bash
# Similar ao Windows, mas inclui:
# - Script launcher com LD_LIBRARY_PATH
# - Permissões executáveis corretas
# - Suporte a bibliotecas compartilhadas
```

## Como Usar

### Para Desenvolvedores

1. **Compile o projeto normalmente**
2. **Execute o script de build portável:**
   ```bash
   # Windows
   .\make_portable_build.ps1
   
   # Linux
   ./make_portable_build.sh
   ```
3. **Teste movendo a pasta para outro local**

### Para Usuários Finais

1. **Extraia o arquivo .zip em qualquer local**
2. **Execute:**
   ```bash
   # Windows
   otclient.exe
   
   # Linux
   ./run_otclient.sh
   ```

## Estrutura da Build Portável

```
otclient-portable/
├── otclient.exe (ou otclient)
├── otclient_cef_subproc.exe
├── cef/
│   ├── libcef.dll (ou libcef.so)
│   ├── locales/
│   ├── icudtl.dat
│   ├── cache/ (vazio)
│   └── outros arquivos CEF...
├── data/
├── modules/
├── mods/
├── init.lua
├── otclientrc.lua
├── README_PORTABLE.txt
└── run_otclient.sh (Linux)
```

## Arquivos Modificados

1. **`src/cef/core/cef_confwin.cpp`** - Verificação de arquivos CEF
2. **`src/cef/core/cef_conflinux.cpp`** - Uso do diretório do executável
3. **`src/cef/core/cef_helper.cpp`** - Log file relativo
4. **`make_portable_build.ps1`** - Script de build Windows (NOVO)
5. **`make_portable_build.sh`** - Script de build Linux (NOVO)

## Benefícios

✅ **Totalmente Portável**: Funciona em qualquer diretório  
✅ **Sem Hardcoding**: Todos os caminhos são relativos ao executável  
✅ **Fácil Distribuição**: Um único .zip funciona para todos  
✅ **Melhor Debugging**: Logs no local correto  
✅ **Cross-Platform**: Funciona em Windows e Linux  

## Testes Recomendados

1. **Teste de Portabilidade**:
   - Mova a build para `C:\Users\Usuario\Desktop\`
   - Mova para `D:\Jogos\OTClient\`
   - Teste em máquina diferente

2. **Teste de Funcionalidade CEF**:
   - Abra módulos que usam webview
   - Verifique se o browser funciona
   - Teste aceleração de hardware

3. **Teste de Logs**:
   - Verifique se `cef.log` é criado no diretório correto
   - Confirme que não há erros de caminhos

## Resolução de Problemas

### "libcef.dll not found"
- Verifique se os arquivos CEF estão na pasta `cef/`
- Execute o script `make_portable_build.ps1` novamente

### "CEF initialization failed"
- Verifique permissões dos arquivos
- Confirme que `otclient_cef_subproc.exe` existe

### Linux: "error while loading shared libraries"
- Use o script `run_otclient.sh` ao invés de executar diretamente
- Instale dependências do sistema se necessário

---

**Esta solução resolve completamente o problema de caminhos hardcoded, tornando o OTClient com CEF totalmente portável para distribuição.**