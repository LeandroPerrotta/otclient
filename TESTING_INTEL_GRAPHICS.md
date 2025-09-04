# Testing Intel Graphics Compatibility

⚠️ **IMPORTANTE:** Esta simulação NÃO reproduz o crash real do Intel. Ela apenas testa se o código de detecção e aplicação de flags funciona.

## Limitações dos Testes

### O Que NÃO Podemos Testar:
- ❌ Crash real do subprocess GPU Intel
- ❌ Problemas específicos dos drivers Intel
- ❌ Incompatibilidades ANGLE + D3D11 + Intel hardware

### O Que PODEMOS Testar:
- ✅ Detecção de placas Intel funciona
- ✅ Flags corretas são aplicadas quando Intel detectada
- ✅ Código de correção é executado
- ✅ Logs aparecem corretamente

## Métodos de Teste (Limitados)

### 1. **Variáveis de Ambiente (Mais Fácil)**

#### Windows (CMD):
```cmd
set CEF_TEST_INTEL_GRAPHICS=1
otclient.exe
```

#### Windows (PowerShell):
```powershell
$env:CEF_TEST_INTEL_GRAPHICS = "1"
.\otclient.exe
```

#### Linux:
```bash
export CEF_TEST_INTEL_GRAPHICS=1
./otclient
```

### 2. **Scripts Automatizados**

#### Windows Batch:
```cmd
test_intel_mode.bat
```

#### PowerShell (Recomendado):
```powershell
# Teste básico
.\test_intel_graphics.ps1 -TestMode

# Teste com override habilitado
.\test_intel_graphics.ps1 -TestMode -AllowOverride
```

### 3. **Variáveis de Ambiente Disponíveis**

| Variável | Valores | Descrição |
|----------|---------|-----------|
| `CEF_TEST_INTEL_GRAPHICS` | `0`, `1`, `true`, `false` | Simula comportamento Intel |
| `CEF_ALLOW_INTEL_OVERRIDE` | `0`, `1`, `true`, `false` | Permite override das configurações Intel |

## O Que Verificar nos Logs

### Quando Intel Mode Ativo:
```
[Windows] TEST MODE: Simulating Intel graphics behavior
[Windows] Intel graphics detected - applying compatibility workarounds
[Windows] Intel compatibility flags applied
[Windows] Command line flags: "otclient.exe" --browser-subprocess-path="..." --disable-d3d11 --disable-gpu-compositing --use-gl=desktop --disable-features=VizDisplayCompositor,D3D11VideoDecoder --enable-features=UseSkiaRenderer --disable-gpu-process-crash-limit --disable-gpu-driver-bug-workarounds --disable-accelerated-2d-canvas --disable-accelerated-video-decode ...
```

### Flags Específicas Intel (devem aparecer):
- `--disable-d3d11`
- `--disable-gpu-compositing`
- `--use-gl=desktop`
- `--disable-features=VizDisplayCompositor,D3D11VideoDecoder`
- `--disable-gpu-process-crash-limit`

### Quando Intel Mode Inativo:
```
[Windows] Command line flags: (sem flags Intel específicas)
```

## Cenários de Teste

### 1. **Teste Básico - Simulação Intel**
```cmd
set CEF_TEST_INTEL_GRAPHICS=1
otclient.exe
```
**Esperado:** Flags Intel aplicadas, sem crash do subprocess GPU.

### 2. **Teste Override - Intel com Override**
```cmd
set CEF_TEST_INTEL_GRAPHICS=1
set CEF_ALLOW_INTEL_OVERRIDE=1
otclient.exe
```
**Esperado:** Comportamento pode variar dependendo da implementação do override.

### 3. **Teste Normal - Sem Simulação**
```cmd
set CEF_TEST_INTEL_GRAPHICS=0
otclient.exe
```
**Esperado:** Comportamento normal, detecção real de GPU.

## Validação da Correção

### ✅ Sucesso:
- Logs mostram detecção Intel (real ou simulada)
- Flags específicas Intel são aplicadas
- CEF inicializa sem crash do subprocess GPU
- Aplicação funciona normalmente

### ❌ Falha:
- Crash do subprocess GPU continua acontecendo
- Flags Intel não aparecem nos logs
- Aplicação não inicia

## Debugging Adicional

Para logs mais detalhados, adicione:
```cmd
set CEF_LOG_SEVERITY=0
set CEF_LOG_FILE=cef_debug.log
```

## Notas Importantes

1. **Ambiente Limpo:** Sempre teste em ambiente limpo sem outras variáveis CEF
2. **Logs Completos:** Salve os logs completos para análise
3. **Comparação:** Compare logs com/sem modo Intel para validar diferenças
4. **Performance:** Modo Intel pode ter performance reduzida (esperado)

## Simulação de Hardware Real

Para simular ainda mais o comportamento real:

### Método 1 - Modificação Temporária do Código:
```cpp
// Em cef_confwin.cpp, force retorno true:
bool CefConfigWindows::isIntelGraphicsSystem() const {
    return true; // Força detecção Intel
}
```

### Método 2 - Mock da Função DXGI:
Crie um wrapper que sempre retorna VendorId Intel (0x8086).

## Teste com Usuário Real

Quando tiver um usuário com Intel graphics:
1. Peça logs completos SEM a correção
2. Aplique a correção
3. Peça logs completos COM a correção
4. Compare os resultados

A correção deve eliminar os erros:
```
[ERROR] GPU process exited unexpectedly: exit_code=-529697949
[FATAL] GPU process isn't usable. Goodbye.
```