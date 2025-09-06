# 🔍 CEF Path Depth Issue - Descoberta Crítica

## 🎯 **Problema Descoberto**

O CEF **não funciona** baseado no caminho específico, mas sim na **profundidade do diretório**!

### ✅ **Funciona (Profundidade 1):**
- `C:\otclient-dev\` 
- `C:\MyGame\`
- `C:\QualquerCoisa\`

### ❌ **Não Funciona (Profundidade 2+):**
- `C:\MyGame\Test\`
- `%HOME%\Downloads\otclient\`
- `C:\Users\Usuario\Desktop\Game\`

## 🧠 **Análise Técnica**

### **Sintomas Observados:**
1. **CEF Initializa**: ✅ Subprocess inicia corretamente
2. **GPU Acceleration**: ✅ Detectado e configurado  
3. **Browser Creation**: ✅ Browser é criado com sucesso
4. **URL Loading**: ✅ Tenta carregar `otclient://webviews/...`
5. **OnPaint Called**: ❌ **NUNCA é chamado em profundidade 2+**
6. **GPU Process Crash**: ❌ `exit_code=-529697949` após alguns minutos

### **Erro Específico:**
```
[4420:9568:0905/213714.784:ERROR:content\browser\gpu\gpu_process_host.cc:959] 
GPU process exited unexpectedly: exit_code=-529697949
```

## 🔍 **Possíveis Causas**

### 1. **Limitações do Windows**
- **MAX_PATH (260 chars)**: Caminhos muito longos podem causar problemas
- **Path depth limits**: Windows pode ter limitações internas de profundidade

### 2. **CEF Sandbox Policies**
- **Security restrictions**: CEF pode ter políticas baseadas na profundidade do diretório
- **Process isolation**: Subprocess pode ter limitações de caminho

### 3. **GPU Process Issues**
- **DirectX/OpenGL context**: Pode ser sensível ao caminho do executável
- **Shared memory**: Problemas de compartilhamento entre processos

### 4. **DLL Loading Issues**
- **AddDllDirectory()**: Pode ter limitações em caminhos profundos
- **Library search paths**: Windows pode ter problemas com caminhos aninhados

## 🛠️ **Soluções Implementadas**

### 1. **Logging Detalhado**
```cpp
// Adicionado logs para detectar o problema
size_t exeDirDepth = std::count(exeDir.begin(), exeDir.end(), L'\\');
logMessage("Windows", stdext::format("Directory depth: %zu levels", exeDirDepth));
```

### 2. **CEF Flags Adicionais**
```cpp
// Flags para contornar problemas de profundidade
command_line->AppendSwitch("disable-gpu-process-crash-limit");
command_line->AppendSwitch("disable-gpu-process-prelaunch");
command_line->AppendSwitch("no-zygote");
command_line->AppendSwitch("disable-dev-shm-usage");
```

### 3. **Error Handling Melhorado**
```cpp
// Verificação de erros no AddDllDirectory
if (cookie == NULL) {
    DWORD error = GetLastError();
    logMessage("Windows", stdext::format("Failed to add CEF directory (Error: %lu)", error));
}
```

## 🧪 **Script de Teste**

Criado `test_path_depth.ps1` para testar sistematicamente:
- Cria builds em diferentes profundidades (1-5 níveis)
- Permite teste manual em cada profundidade
- Ajuda a identificar o limite exato

## 📋 **Próximos Passos**

### **Para Teste:**
1. **Execute**: `.\test_path_depth.ps1`
2. **Teste cada profundidade** manualmente
3. **Documente** exatamente onde para de funcionar
4. **Verifique logs** em `cef.log`

### **Possíveis Soluções:**

#### **Opção 1: Workaround de Caminho**
```cpp
// Criar symlink ou junction para caminho mais curto
// CreateSymbolicLink() ou CreateJunction()
```

#### **Opção 2: Subprocess Relocation**
```cpp
// Copiar subprocess para local temporário mais simples
// Ex: C:\Temp\otclient_cef_subproc.exe
```

#### **Opção 3: Registry/Environment**
```cpp
// Configurar variáveis de ambiente para CEF
// Forçar caminhos específicos via registry
```

#### **Opção 4: CEF Build Custom**
```cpp
// Compilar CEF com flags específicas
// Remover limitações de caminho
```

## 🎯 **Conclusão**

Este é um **bug muito específico** relacionado à profundidade de diretórios no Windows. Não é um problema de:
- ❌ Caminhos relativos vs absolutos
- ❌ Working directory vs executable directory  
- ❌ Arquivos faltando
- ❌ Configuração incorreta

É um problema de **limitação de profundidade de caminho** no CEF/Windows que precisa de uma solução específica.

---

**Esta descoberta muda completamente nossa abordagem para resolver o problema.**